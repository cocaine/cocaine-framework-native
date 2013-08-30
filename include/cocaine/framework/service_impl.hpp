#ifndef COCAINE_FRAMEWORK_SERVICE_IMPL_HPP
#define COCAINE_FRAMEWORK_SERVICE_IMPL_HPP

#include <cocaine/asio/tcp.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/rpc/channel.hpp>

#include <memory>
#include <string>
#include <mutex>

namespace cocaine { namespace framework {

class service_connection_t :
    public std::enable_shared_from_this<service_connection_t>
{
    COCAINE_DECLARE_NONCOPYABLE(service_connection_t)

    friend class service_manager_t;

public:
    typedef cocaine::io::channel<cocaine::io::socket<cocaine::io::tcp>>
            iochannel_t;

    typedef uint64_t
            session_id_t;

    typedef std::pair<std::string, uint16_t>
            endpoint_t;

public:
    ~service_connection_t();

    std::string
    name() const;

    int
    version() const {
        return m_version;
    }

    service_status
    status() const {
        return m_connection_status;
    }

    void
    set_timeout(float timeout);

    // returns empty pointer if the manager doesn't exist
    std::shared_ptr<service_manager_t>
    get_manager() throw();

    template<class Event, typename... Args>
    typename service_traits<Event>::future_type
    call(Args&&... args);

    future<std::shared_ptr<service_connection_t>>
    reconnect();

    void
    soft_destroy();

private:
    service_connection_t(const std::string& name,
                         std::shared_ptr<service_manager_t> manager,
                         unsigned int version);

    service_connection_t(const std::vector<endpoint_t>& endpoints,
                         std::shared_ptr<service_manager_t> manager,
                         unsigned int version);

    // throws if the manager doesn't exist
    std::shared_ptr<service_manager_t>
    manager();

    session_id_t
    create_session(std::shared_ptr<detail::service::service_handler_concept_t> handler,
                   std::shared_ptr<iochannel_t>& channel);

    future<std::shared_ptr<service_connection_t>>
    connect();

    void
    connect_to_endpoint();

    void
    disconnect(service_status status = service_status::disconnected);

    void
    reset_sessions();

    std::shared_ptr<service_connection_t>
    on_resolved(service_traits<cocaine::io::locator::resolve>::future_type&);

    void
    on_message(const cocaine::io::message_t& message);

    void
    on_error(const std::error_code&);

private:
    struct session_data_t {
        void
        on_timeout(ev::timer&, int);

        session_id_t session;
        std::shared_ptr<detail::service::service_handler_concept_t> handler;
        std::shared_ptr<ev::timer> close_timer;
        service_connection_t *connection;
    };
    friend struct session_data_t;

    typedef std::map<session_id_t, session_data_t>
            handlers_map_t;

    boost::optional<std::string> m_name;
    std::vector<endpoint_t> m_endpoints;
    unsigned int m_version;

    std::atomic<session_id_t> m_session_counter;
    handlers_map_t m_handlers;
    std::recursive_mutex m_handlers_lock;

    std::weak_ptr<service_manager_t> m_manager;
    std::shared_ptr<iochannel_t> m_channel;

    service_status m_connection_status;
    bool m_dying;

    // if service does not respond within m_timeout seconds, the session will be deleted
    boost::optional<float> m_timeout;
};

template<class Event, typename... Args>
typename service_traits<Event>::future_type
service_connection_t::call(Args&&... args) {
    // prepare future
    auto h = std::make_shared<detail::service::service_handler<Event>>();
    auto f = h->get_future();

    // create session
    std::shared_ptr<iochannel_t> channel;
    session_id_t current_session;
    try {
        current_session = this->create_session(h, channel);
    } catch (...) {
        h->error(std::current_exception());
        return f;
    }

    // send the request
    channel->wr->write<Event>(current_session, std::forward<Args>(args)...);

    return f;
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_IMPL_HPP
