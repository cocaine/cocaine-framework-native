#ifndef COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP
#define COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/logging.hpp>

#include <cocaine/essentials/services/logging.hpp>

namespace cocaine { namespace framework {

class logging_service_t :
    public service_t,
    public logger_t
{
public:
    logging_service_t(const std::string& name,
                      cocaine::io::reactor_t& service,
                    const cocaine::io::tcp::endpoint& resolver) :
        service_t(name, service, resolver)
    {
        // pass
    }

    void
    emit(cocaine::logging::priorities priority,
         const std::string& source,
         const std::string& message)
    {
        call<cocaine::io::logging::emit>(ignore_message,
                                         static_cast<int>(priority),
                                         source,
                                         message);
    }

    cocaine::logging::priorities
    verbosity() const {
        return cocaine::logging::priorities::debug;
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP
