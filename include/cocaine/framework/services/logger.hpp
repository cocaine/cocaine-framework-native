#ifndef COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP
#define COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP

#include <cocaine/framework/service.hpp>

#include <cocaine/forwards.hpp>
#include <cocaine/format.hpp>

#define COCAINE_LOG(_log_, _level_, ...) \
    do { if ((_log_)->verbosity() >= (_level_)) (_log_)->emit((_level_), __VA_ARGS__); } while (false)

#define COCAINE_LOG_DEBUG(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::debug, __VA_ARGS__)

#define COCAINE_LOG_INFO(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::info, __VA_ARGS__)

#define COCAINE_LOG_WARNING(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::warning, __VA_ARGS__)

#define COCAINE_LOG_ERROR(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::error, __VA_ARGS__)

namespace cocaine { namespace framework {

class logging_service_t :
    public std::enable_shared_from_this<logging_service_t>,
    public service_t
{
public:
    logging_service_t(const std::string& name,
                      cocaine::io::reactor_t& service,
                      const cocaine::io::tcp::endpoint& resolver,
                      const std::string& source) :
        service_t(name, service, resolver),
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

    template<typename... Args>
    void
    emit(cocaine::logging::priorities priority,
         const std::string& format,
         const Args&... args)
    {
        emit(priority, cocaine::format(format, args...));
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

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP
