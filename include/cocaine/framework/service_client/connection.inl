#ifndef COCAINE_FRAMEWORK_SERVICE_CONNECTION_INL
#define COCAINE_FRAMEWORK_SERVICE_CONNECTION_INL

#include <ev++.h>

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
        status() != service_status::connecting &&
        m_auto_reconnect)
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
