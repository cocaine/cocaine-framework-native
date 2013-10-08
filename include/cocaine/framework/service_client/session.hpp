#ifndef COCAINE_FRAMEWORK_SERVICE_SESSION_HPP
#define COCAINE_FRAMEWORK_SERVICE_SESSION_HPP

#include <cocaine/framework/service_client/handler.hpp>

#include <memory>

namespace cocaine { namespace framework {

typedef uint64_t
        session_id_t;

class service_connection_t;

template<class Event>
class session {
    COCAINE_DECLARE_NONCOPYABLE(session)

public:
    typedef typename detail::service::service_handler<Event>::future_type
            future_type;

public:
    session(const std::shared_ptr<service_connection_t>& client,
            session_id_t id,
            future_type&& downstream);

    session(session&& other);

    session&
    operator=(session&& other);

    session_id_t
    id() const;

    void
    set_timeout(float seconds);

    future_type&
    downstream();

private:
    std::shared_ptr<service_connection_t> m_client;
    session_id_t m_id;
    future_type m_downstream;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_SESSION_HPP
