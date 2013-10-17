#ifndef COCAINE_FRAMEWORK_SERVICE_CONNECTION_HPP
#define COCAINE_FRAMEWORK_SERVICE_CONNECTION_HPP

#include <cocaine/framework/service_client/session.hpp>
#include <cocaine/framework/service_client/status.hpp>

#include <cocaine/asio/tcp.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/rpc/channel.hpp>

#include <memory>
#include <string>
#include <mutex>

namespace cocaine { namespace framework {

class service_manager_t;

namespace detail { namespace service {

    class session_data_t;

}} // namespace detail::service

class service_connection_t :
    public std::enable_shared_from_this<service_connection_t>
{
    COCAINE_DECLARE_NONCOPYABLE(service_connection_t)

public:
    typedef std::pair<std::string, uint16_t>
            endpoint_t;

public:
    service_connection_t(const std::string& name,
                         std::shared_ptr<service_manager_t> manager,
                         size_t thread,
                         unsigned int version);

    service_connection_t(const std::vector<endpoint_t>& endpoints,
                         std::shared_ptr<service_manager_t> manager,
                         size_t thread,
                         unsigned int version);

    // must be deleted from service thread
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
    
    size_t
    thread() const;

    // returns empty pointer if the manager doesn't exist
    std::shared_ptr<service_manager_t>
    get_manager() throw();

    template<class Event, typename... Args>
    session<Event>
    call(Args&&... args);

    void
    delete_session(session_id_t id, service_errc ec);

    void
    set_timeout(session_id_t id, float seconds);

    // call it only when disconnected!
    future<std::shared_ptr<service_connection_t>>
    connect();

    // must be called only from service thread
    void
    disconnect(service_status status = service_status::disconnected);

    size_t
    footprint() const;

    size_t
    sessions_count() const;

private:
    // throws if the manager doesn't exist
    std::shared_ptr<service_manager_t>
    manager();

    std::shared_ptr<service_connection_t>
    connect_to_endpoint();

    std::shared_ptr<service_connection_t>
    on_resolved(session<cocaine::io::locator::resolve>::future_type&);

    void
    on_error(const std::error_code&);

    void
    on_message(const cocaine::io::message_t& message);

    void
    set_timeout_impl(session_id_t id, float seconds);

private:
    typedef std::map<session_id_t, detail::service::session_data_t>
            sessions_map_t;

    typedef cocaine::io::channel<cocaine::io::socket<cocaine::io::tcp>>
            iochannel_t;

    boost::optional<std::string> m_name;
    std::vector<endpoint_t> m_endpoints;
    unsigned int m_version;

    std::atomic<session_id_t> m_session_counter;
    sessions_map_t m_sessions;
    mutable std::mutex m_sessions_mutex;

    std::weak_ptr<service_manager_t> m_manager;
    size_t m_thread;
    iochannel_t m_channel;

    service_status m_connection_status;
};

}} // namespace cocaine::framework

#include <cocaine/framework/service_client/connection.inl>
#include <cocaine/framework/service_client/session.inl>

#endif // COCAINE_FRAMEWORK_SERVICE_CONNECTION_HPP
