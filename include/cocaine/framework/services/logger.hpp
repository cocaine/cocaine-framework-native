#ifndef COCAINE_FRAMEWORK_SERVICES_LOGGING_HPP
#define COCAINE_FRAMEWORK_SERVICES_LOGGING_HPP

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/logging.hpp>

namespace cocaine { namespace framework {

struct logging_service_t :
    public service_stub_t,
    public logger_t
{
    static const unsigned int version = cocaine::io::protocol<cocaine::io::logging_tag>::version::value;

    logging_service_t(std::shared_ptr<service_t> service,
                      const std::string& source) :
        service_stub_t(service),
        m_source(source),
        m_priority(cocaine::logging::debug/*backend()->call<cocaine::io::logging::verbosity>().get()*/)
    {
        // pass
    }

    void
    emit(cocaine::logging::priorities priority,
         const std::string& message)
    {
        backend()->call<cocaine::io::logging::emit>(priority, m_source, message);
    }

    cocaine::logging::priorities
    verbosity() const {
        return m_priority;
    }

private:
    std::string m_source;
    cocaine::logging::priorities m_priority;
};

}} // cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_LOGGING_HPP
