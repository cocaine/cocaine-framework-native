#ifndef COCAINE_FRAMEWORK_DISPATCH_HPP
#define COCAINE_FRAMEWORK_DISPATCH_HPP

#include <cocaine/framework/common.hpp>
#include <cocaine/framework/service.hpp>
#include <cocaine/framework/handler.hpp>
#include <cocaine/framework/handlers/functional.hpp>

#include <cocaine/asio/local.hpp>
#include <cocaine/asio/reactor.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/rpc/channel.hpp>
#include <cocaine/format.hpp>

#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <ev++.h>

namespace cocaine { namespace framework {

namespace detail {
    class dispatch_upstream_t;

    namespace cf = cocaine::framework;

    template<class Pointer, class Method>
    struct method_handler_traits {
        typedef decltype(cf::template declval<Pointer>()
                         ->cf::template declval<Method>()(std::string(),
                                                          std::vector<std::string>(),
                                                          response_ptr()))
                result_type;

        typedef typename std::enable_if<std::is_same<void, result_type>::value>::type
                void_t;
    };
}

class dispatch_t {
    COCAINE_DECLARE_NONCOPYABLE(dispatch_t)

    friend class detail::dispatch_upstream_t;

    template<class App, typename... Args>
    friend int run(int argc, char *argv[], Args&&... args);

public:
    std::shared_ptr<service_manager_t>
    service_manager() const {
        return m_service_manager;
    }

    const std::string&
    id() const {
        return m_id;
    }

    template<class Pointer, class T>
    void
    on(const std::string& event,
       Pointer obj,
       void(T::*method)(const std::string&, const std::vector<std::string>&, response_ptr));

    template<class Handler, class App>
    typename std::enable_if<std::is_base_of<handler<App>, Handler>::value>::type
    on(const std::string& event, App &app);

    template<class Factory, class... Args>
    typename std::enable_if<std::is_base_of<basic_factory_t, Factory>::value>::type
    on(const std::string& event, Args&&... args);

private:
    dispatch_t(const std::string& name,
               const std::string& uuid,
               const std::string& endpoint,
               uint16_t resolver_port);

    static
    std::unique_ptr<dispatch_t>
    create(int argc, char *argv[]);

    void
    run();

    template<class Event, typename... Args>
    void
    send(Args&&... args);

    void
    on(const std::string& event,
       std::shared_ptr<basic_factory_t> factory);

    std::shared_ptr<basic_handler_t>
    invoke(const std::string& event,
           std::shared_ptr<upstream_t> response);

    void
    on_message(const cocaine::io::message_t& message);

    void
    on_heartbeat(ev::timer&, int);

    void
    on_disown(ev::timer&, int);

    void
    terminate(int code,
              const std::string& reason);

private:
    struct io_pair_t {
        std::shared_ptr<upstream_t> upstream;
        std::shared_ptr<basic_handler_t> handler;
    };

    typedef std::map<uint64_t, io_pair_t>
            stream_map_t;

    typedef std::map<std::string, std::shared_ptr<basic_factory_t>>
            handlers_map;

private:
    const std::string m_id;

    cocaine::io::reactor_t m_ioservice;
    ev::timer m_heartbeat_timer,
              m_disown_timer;
    std::shared_ptr<
        cocaine::io::channel<
            cocaine::io::socket<cocaine::io::local>
    >> m_channel;
    std::mutex m_send_lock;

    std::shared_ptr<service_manager_t> m_service_manager;

    handlers_map m_handlers;
    stream_map_t m_sessions;
};

template<class Event, typename... Args>
void
dispatch_t::send(Args&&... args) {
    std::lock_guard<std::mutex> lock(m_send_lock);
    m_channel->wr->write<Event>(std::forward<Args>(args)...);
}

template<class Pointer, class T>
void
dispatch_t::on(
    const std::string& event,
    Pointer obj,
    void(T::*method)(const std::string&, const std::vector<std::string>&, response_ptr)
)
{
    namespace ph = std::placeholders;
    this->on<function_factory_t>(
        event,
        std::bind(method, obj, ph::_1, ph::_2, ph::_3)
    );
}

template<class Handler, class App>
typename std::enable_if<std::is_base_of<handler<App>, Handler>::value>::type
dispatch_t::on(const std::string& event,
               App &app)
{
    this->on<handler_factory<Handler>>(event, app);
}

template<class Factory, class... Args>
typename std::enable_if<std::is_base_of<basic_factory_t, Factory>::value>::type
dispatch_t::on(const std::string& event,
               Args&&... args)
{
    this->on(event, std::make_shared<Factory>(std::forward<Args>(args)...));
}

template<class App, typename... Args>
int
run(int argc,
    char *argv[],
    Args&&... args)
{
    // i'm not self-assured
    std::unique_ptr<dispatch_t> dispatch;
    try {
        dispatch = dispatch_t::create(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "ERROR: Unknown error!" << std::endl;
        return 1;
    }

    try {
        App app(*dispatch.get(), std::forward<Args>(args)...);

        try {
            dispatch->run();
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "ERROR: " << e.what() << std::endl;
            return 1;
        } catch (...) {
            std::cerr << "ERROR: Unknown error!" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        dispatch->terminate(
            cocaine::io::rpc::terminate::abnormal,
            cocaine::format("Error has occurred while initializing the application: %s", e.what())
        );

        std::cerr << "ERROR: " << e.what() << std::endl;

        return 1;
    } catch (...) {
        dispatch->terminate(
            cocaine::io::rpc::terminate::abnormal,
            "Unknown error has occurred while initializing the application"
        );

        std::cerr << "ERROR: Unknown error!" << std::endl;

        return 1;
    }
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_DISPATCH_HPP
