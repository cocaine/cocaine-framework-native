#ifndef COCAINE_FRAMEWORK_WORKER_HPP
#define COCAINE_FRAMEWORK_WORKER_HPP

#include <cocaine/framework/basic_application.hpp>
#include <cocaine/framework/upstream.hpp>
#include <cocaine/framework/service.hpp>

#include <cocaine/asio/local.hpp>
#include <cocaine/asio/reactor.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/rpc/channel.hpp>
#include <cocaine/format.hpp>

#include <string>
#include <map>
#include <memory>
#include <ev++.h>

namespace cocaine { namespace framework {

class worker_t {
    COCAINE_DECLARE_NONCOPYABLE(worker_t)

public:
    template<class App, typename... Args>
    static
    int
    run(int argc, char *argv[], Args&&...);

    ~worker_t();

    template<class Event, typename... Args>
    void
    send(Args&&... args);

private:
    struct io_pair_t {
        std::shared_ptr<upstream_t> upstream;
        std::shared_ptr<basic_handler_t> handler;
    };

    typedef std::map<uint64_t, io_pair_t> stream_map_t;

private:
    static
    std::shared_ptr<worker_t>
    create(int argc, char *argv[]);

    worker_t(const std::string& name,
             const std::string& uuid,
             const std::string& endpoint);

    template<class App, typename... Args>
    void
    create_application(Args&&... args);

    int
    run();

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
    const std::string m_id;
    cocaine::io::reactor_t m_ioservice;
    ev::timer m_heartbeat_timer,
              m_disown_timer;
    std::shared_ptr<
        cocaine::io::channel<
            cocaine::io::socket<cocaine::io::local>
    >> m_channel;
    std::shared_ptr<service_manager_t> m_service_manager;

    std::string m_app_name;
    std::shared_ptr<basic_application_t> m_application;

    std::shared_ptr<logging_service_t> m_log;

    stream_map_t m_streams;
};

template<class Event, typename... Args>
void
worker_t::send(Args&&... args) {
    m_channel->wr->write<Event>(std::forward<Args>(args)...);
}

template<class App, typename... Args>
void
worker_t::create_application(Args&&... args) {
    try {
        auto app = std::make_shared<App>(m_app_name,
                                         m_service_manager,
                                         std::forward<Args>(args)...);
        app->initialize();
        m_application = app;
    } catch (const std::exception& e) {
        terminate(cocaine::io::rpc::terminate::abnormal,
                  cocaine::format("Error has occurred while initializing the application: %s", e.what()));
        throw;
    } catch (...) {
        terminate(cocaine::io::rpc::terminate::abnormal,
                  "Unknown error has occurred while initializing the application.");
        throw;
    }
}

template<class App, typename... Args>
int
worker_t::run(int argc,
              char *argv[],
              Args&&... args) {
    std::shared_ptr<worker_t> worker;
    try {
        worker = create(argc, argv);
    } catch (int e) {
        return e;
    }
    worker->create_application<App>(std::forward<Args>(args)...);
    return worker->run();
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_WORKER_HPP
