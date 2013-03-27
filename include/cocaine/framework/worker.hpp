#ifndef COCAINE_FRAMEWORK_WORKER_HPP
#define COCAINE_FRAMEWORK_WORKER_HPP

#include <string>
#include <map>
#include <memory>
#include <boost/utility.hpp>
#include <ev++.h>
#include <cocaine/api/stream.hpp>
#include <cocaine/asio/local.hpp>
#include <cocaine/asio/reactor.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/rpc/channel.hpp>

#include <cocaine/framework/application.hpp>
#include <cocaine/framework/logging.hpp>
#include <cocaine/framework/service.hpp>

namespace cocaine { namespace framework {

class worker_t :
    public boost::noncopyable
{
    struct io_pair_t {
        std::shared_ptr<cocaine::api::stream_t> upstream;
        std::shared_ptr<cocaine::api::stream_t> downstream;
    };

    typedef std::map<uint64_t, io_pair_t> stream_map_t;

public:
    worker_t(const std::string& name,
             const std::string& uuid,
             const std::string& endpoint);

    ~worker_t();

    void
    run();

    template<class AppT>
    void
    add(const std::string& name, const AppT& a);

    template<class Event, typename... Args>
    void
    send(Args&&... args);

private:
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
    cocaine::io::reactor_t m_service;
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

template<class AppT>
void
worker_t::add(const std::string& name, const AppT& a) {
    if (name == m_app_name) {
        m_application.reset(new AppT(a));
        m_application->initialize(name, m_service_manager);
    }
}

std::shared_ptr<worker_t>
make_worker(int argc,
            char *argv[]);

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_WORKER_HPP