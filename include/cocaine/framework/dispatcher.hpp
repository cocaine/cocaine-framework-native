#ifndef COCAINE_FRAMEWORK_DISPATCHER_HPP
#define COCAINE_FRAMEWORK_DISPATCHER_HPP

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
    class dispatcher_upstream_t;

    namespace cf = cocaine::framework;

    template<class Pointer, class Method>
    struct method_handler_traits {
        typedef decltype(cf::declval<Pointer>()->cf::declval<Method>()(std::string(),
                                                                       std::vector<std::string>(),
                                                                       response_ptr()))
                result_type;

        typedef typename std::enable_if<std::is_same<void, result_type>::value>::type
                void_t;
    };
}

class dispatcher_t {
    COCAINE_DECLARE_NONCOPYABLE(dispatcher_t)

    friend class detail::dispatcher_upstream_t;

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

    void
    on(const std::string& event,
       std::shared_ptr<basic_factory_t> factory);

    template<class Pointer, class T>
    void
    on(const std::string& event,
       Pointer obj,
       void(T::*method)(const std::string&, const std::vector<std::string>&, response_ptr));

    template<class Handler, class App>
    typename std::enable_if<std::is_base_of<handler<App>, Handler>::value>::type
    on(const std::string& event, App *app);

    template<class Factory, class... Args>
    typename std::enable_if<std::is_base_of<basic_factory_t, Factory>::value>::type
    on(const std::string& event, Args&&... args);

    void
    on_unregistered(std::shared_ptr<basic_factory_t> factory);

    template<class Pointer, class T>
    void
    on_unregistered(Pointer obj,
                    void(T::*method)(const std::string&, const std::vector<std::string>&, response_ptr));

    template<class Handler, class App>
    typename std::enable_if<std::is_base_of<handler<App>, Handler>::value>::type
    on_unregistered(App *app);

    template<class Factory, class... Args>
    typename std::enable_if<std::is_base_of<basic_factory_t, Factory>::value>::type
    on_unregistered(Args&&... args);

private:
    dispatcher_t(const std::string& name,
                 const std::string& uuid,
                 const std::string& endpoint,
                 uint16_t resolver_port);

    static
    std::shared_ptr<dispatcher_t>
    create(int argc, char *argv[]);

    void
    run();

    template<class Event, typename... Args>
    void
    send(Args&&... args);

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
    typedef std::map<std::string, std::shared_ptr<basic_factory_t>>
            handlers_map;

    struct io_pair_t {
        std::shared_ptr<upstream_t> upstream;
        std::shared_ptr<basic_handler_t> handler;
    };

    typedef std::map<uint64_t, io_pair_t>
            stream_map_t;

private:
    const std::string m_id;
    const std::string m_app_name;

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
    std::shared_ptr<basic_factory_t> m_default_handler;

    stream_map_t m_sessions;
};

template<class Event, typename... Args>
void
dispatcher_t::send(Args&&... args) {
    std::lock_guard<std::mutex> lock(m_send_lock);
    m_channel->wr->write<Event>(std::forward<Args>(args)...);
}

template<class Pointer, class T>
void
dispatcher_t::on(
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
dispatcher_t::on(const std::string& event,
                 App *app)
{
    this->on<handler_factory<Handler>>(event, app);
}

template<class Factory, class... Args>
typename std::enable_if<std::is_base_of<basic_factory_t, Factory>::value>::type
dispatcher_t::on(const std::string& event,
                 Args&&... args)
{
    this->on(event, std::make_shared<Factory>(std::forward<Args>(args)...));
}

template<class Pointer, class T>
void
dispatcher_t::on_unregistered(
    Pointer obj,
    void(T::*method)(const std::string&, const std::vector<std::string>&, response_ptr)
)
{
    namespace ph = std::placeholders;
    this->on_unregistered<function_factory_t>(
        std::bind(method, obj, ph::_1, ph::_2, ph::_3)
    );
}

template<class Handler, class App>
typename std::enable_if<std::is_base_of<handler<App>, Handler>::value>::type
dispatcher_t::on_unregistered(App *app)
{
    this->on_unregistered<handler_factory<Handler>>(app);
}

template<class Factory, class... Args>
typename std::enable_if<std::is_base_of<basic_factory_t, Factory>::value>::type
dispatcher_t::on_unregistered(Args&&... args)
{
    this->on_unregistered(std::make_shared<Factory>(std::forward<Args>(args)...));
}

template<class App, typename... Args>
int
run(int argc,
    char *argv[],
    Args&&... args)
{
    std::shared_ptr<dispatcher_t> dispatcher;
    try {
        dispatcher = dispatcher_t::create(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "ERROR: Unknown error!" << std::endl;
        return 1;
    }

    std::shared_ptr<App> app;
    try {
        app = std::make_shared<App>(std::forward<Args>(args)...);
        app->init(dispatcher.get());
    } catch (const std::exception& e) {
        dispatcher->terminate(
            cocaine::io::rpc::terminate::abnormal,
            cocaine::format("Error has occurred while initializing the application: %s", e.what())
        );
        throw;
    } catch (...) {
        dispatcher->terminate(
            cocaine::io::rpc::terminate::abnormal,
            "Unknown error has occurred while initializing the application"
        );
        throw;
    }

    try {
        dispatcher->run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_DISPATCHER_HPP
