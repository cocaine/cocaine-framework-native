#ifndef COCAINE_FRAMEWORK_SERVICE_BASE_HPP
#define COCAINE_FRAMEWORK_SERVICE_BASE_HPP

#include <cocaine/framework/service_client/connection.hpp>

#include <memory>
#include <string>

namespace cocaine { namespace framework {

template<class Event>
struct service_traits {
    typedef typename detail::service::service_handler<Event>::future_type
            future_type;

    typedef typename detail::service::service_handler<Event>::promise_type
            promise_type;
};

class service_t {
    COCAINE_DECLARE_NONCOPYABLE(service_t)

public:
    service_t(std::shared_ptr<service_connection_t> connection);

    virtual
    ~service_t();

    std::string
    name();

    service_status
    status() const;

    void
    set_timeout(float timeout);

    template<class Event, typename... Args>
    typename session<Event>::future_type
    call(Args&&... args);

    void
    soft_destroy();

private:
    std::shared_ptr<service_connection_t> m_connection;
    boost::optional<float> m_timeout;
};

}} // namespace cocaine::framework

template<class Event, typename... Args>
typename cocaine::framework::session<Event>::future_type
cocaine::framework::service_t::call(Args&&... args) {
    auto session = m_connection->call<Event, Args...>(std::forward<Args>(args)...);

    if (m_timeout) {
        session.set_timeout(*m_timeout);
    }

    return std::move(session.downstream());
}

#endif // COCAINE_FRAMEWORK_SERVICE_BASE_HPP
