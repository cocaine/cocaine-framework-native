#ifndef COCAINE_FRAMEWORK_SERVICE_CONNECTION_INL
#define COCAINE_FRAMEWORK_SERVICE_CONNECTION_INL

#include <ev++.h>

namespace cocaine { namespace framework {

class service_connection_t;

namespace detail { namespace service {

    class session_data_t {
    public:
        session_data_t();

        session_data_t(const std::shared_ptr<service_connection_t>& connection,
                       session_id_t id,
                       std::shared_ptr<detail::service::service_handler_concept_t>&& handler);

        ~session_data_t();

        void
        set_timeout(float seconds);

        void
        stop_timer();

        detail::service::service_handler_concept_t*
        handler() const {
            return m_handler.get();
        }

    private:
        void
        on_timeout(ev::timer&, int);

    private:
        session_id_t m_id;
        std::shared_ptr<detail::service::service_handler_concept_t> m_handler;
        std::shared_ptr<ev::timer> m_close_timer;
        bool m_stopped;
        std::shared_ptr<service_connection_t> m_connection;
    };

}} // namespace detail::service

}} // namespace cocaine::framework

template<class Event, typename... Args>
cocaine::framework::session<Event>
cocaine::framework::service_connection_t::call(Args&&... args) {
    auto h = std::make_shared<detail::service::service_handler<Event>>();
    //auto h = std::unique_ptr<detail::service::service_handler<Event>>(new detail::service::service_handler<Event>);
    auto f = h->get_future();

    session_id_t current_session = m_session_counter++;

    std::unique_lock<std::mutex> lock(m_sessions_mutex);

    // try to connect if disconnected
    if (status() != service_status::connected &&
        status() != service_status::connecting)
    {
        connect();
    }

    if (status() == service_status::connected ||
        status() == service_status::connecting)
    {
        m_sessions[current_session] = detail::service::session_data_t(shared_from_this(),
                                                                      current_session,
                                                                      std::move(h));

        m_channel.wr->write<Event>(current_session, std::forward<Args>(args)...);
    } else {
        lock.unlock();
        // for now 'connect' may fail immediately only when service manager doesn't exist
        h->error(
            cocaine::framework::make_exception_ptr(service_error_t(service_errc::broken_manager))
        );
    }

    return session<Event>(shared_from_this(), current_session, std::move(f));
}

#endif // COCAINE_FRAMEWORK_SERVICE_CONNECTION_INL
