#ifndef COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP
#define COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/logging.hpp>

namespace cocaine { namespace framework {

class logging_service_t :
    public std::enable_shared_from_this<logging_service_t>,
    public service_t,
    public logger_t
{
public:
    logging_service_t(const std::string& name,
                      cocaine::io::reactor_t& service,
                      const cocaine::io::tcp::endpoint& resolver) :
        service_t(name, service, resolver),
        m_priority(cocaine::logging::priorities::warning)
    {
        // pass
    }

    void
    emit(cocaine::logging::priorities priority,
         const std::string& source,
         const std::string& message)
    {
        call<cocaine::io::logging::emit>(priority,
                                         source,
                                         message);
    }

    cocaine::logging::priorities
    verbosity() const {
        return m_priority;
    }

    void
    initialize();

protected:
    void
    on_verbosity_response(cocaine::io::reactor_t *ioservice,
                          const cocaine::io::message_t& message);

private:
    cocaine::logging::priorities m_priority;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP
