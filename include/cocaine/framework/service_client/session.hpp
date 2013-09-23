#ifndef COCAINE_FRAMEWORK_SERVICE_SESSION_HPP
#define COCAINE_FRAMEWORK_SERVICE_SESSION_HPP

#include <memory>

namespace cocaine { namespace framework {

template<class Event>
class session {
    COCAINE_DECLARE_NONCOPYABLE(session)

public:
    typedef typename detail::service::service_handler<Event>::future_type
            future_type;

public:
    session(const std::shared_ptr<service_connection_t>& client,
            session_id_t id,
            future_type&& downstream) :
        m_client(client),
        m_id(id),
        m_downstream(std::move(downstream))
    {
        // pass
    }

    session(session&& other) :
        m_client(std::move(other.m_client)),
        m_id(other.m_id),
        m_downstream(std::move(other.m_downstream))
    {
        // pass
    }

    session&
    operator=(session&& other) {
        m_client = std::move(other.m_client);
        m_id = other.m_id;
        m_downstream = std::move(other.m_downstream);

        return *this;
    }

    session_id_t
    id() const {
        return m_id;
    }

    void
    set_timeout(float seconds);

    future_type&
    downstream() {
        return m_downstream;
    }

private:
    std::shared_ptr<service_connection_t> m_client;
    session_id_t m_id;
    future_type m_downstream;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_SESSION_HPP
