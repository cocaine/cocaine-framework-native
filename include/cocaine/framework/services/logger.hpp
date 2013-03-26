#ifndef COCAINE_FRAMEWORK_LOGGER_HPP
#define COCAINE_FRAMEWORK_LOGGER_HPP

#include <cocaine/essentials/services/logging.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/logging.hpp>

namespace cocaine { namespace framework {

struct logging_service :
    public stub,
    public logger_t
{
    logging_service(const cocaine::io::tcp::endpoint& endpoint,
                    cocaine::io::reactor_t& service) :
        stub(endpoint, service)
    {
        // pass
    }

    void
    emit(cocaine::logging::priorities priority,
         const std::string& source,
         const std::string& message)
    {
        call<cocaine::io::logging::emit>(static_cast<int>(priority), source, message);
    }

    cocaine::logging::priorities
    verbosity() const {
        return cocaine::logging::priorities::debug;
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_LOGGER_HPP
