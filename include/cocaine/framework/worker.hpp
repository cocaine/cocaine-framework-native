#ifndef COCAINE_FRAMEWORK_WORKER_HPP
#define COCAINE_FRAMEWORK_WORKER_HPP

#include <cocaine/framework/application.hpp>
#include <cocaine/framework/logging.hpp>
#include <cocaine/framework/service.hpp>

#include <cocaine/api/stream.hpp>
#include <cocaine/asio/local.hpp>
#include <cocaine/asio/reactor.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/rpc/channel.hpp>

#include <string>
#include <map>
#include <memory>
#include <boost/utility.hpp>
#include <ev++.h>

namespace cocaine { namespace framework {

class worker_t :
    private boost::noncopyable
{
public:
    static
    std::shared_ptr<worker_t>
    create(int argc, char *argv[]);

public:
    ~worker_t();

    void
    run();

    template<class App, typename... Args>
    void
    create_application(Args&&... args);

    std::shared_ptr<application_t>
    get_application() const {
        return m_application;
    }

    template<class Event, typename... Args>
    void
    send(Args&&... args);

private:
    struct io_pair_t {
        std::shared_ptr<cocaine::api::stream_t> upstream;
        std::shared_ptr<base_handler_t> downstream;
    };

    typedef std::map<uint64_t, io_pair_t> stream_map_t;

private:
    worker_t(const std::string& name,
             const std::string& uuid,
             const std::string& endpoint);

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
    std::shared_ptr<application_t> m_application;

    std::shared_ptr<log_t> m_log;

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
    auto app = std::make_shared<App>(name, m_service_manager, std::forward<Args>(args)...);
    app->initialize();
    m_application = app;
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_WORKER_HPP
