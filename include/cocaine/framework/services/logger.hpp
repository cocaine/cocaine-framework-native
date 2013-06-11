#ifndef COCAINE_FRAMEWORK_SERVICES_LOGGING_HPP
#define COCAINE_FRAMEWORK_SERVICES_LOGGING_HPP

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
                      cocaine::io::reactor_t& working_service,
                      const executor_t& executor,
                      const cocaine::io::tcp::endpoint& resolver,
                      std::shared_ptr<logger_t> logger, // not so much OMG
                      const std::string& source) :
        service_t(name,
                  working_service,
                  executor,
                  resolver,
                  logger,
                  cocaine::io::protocol<cocaine::io::logging_tag>::version::value),
        m_priority(cocaine::logging::priorities::warning),
        m_source(source)
    {
        // pass
    }

    void
    emit(cocaine::logging::priorities priority,
         const std::string& message)
    {
        call<cocaine::io::logging::emit>(priority, m_source, message);
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
    std::string m_source;
};

}} // cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_LOGGING_HPP
