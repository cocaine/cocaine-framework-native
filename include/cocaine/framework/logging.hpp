#ifndef COCAINE_FRAMEWORK_LOGGING_HPP
#define COCAINE_FRAMEWORK_LOGGING_HPP

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

class logger_t
{
public:
    virtual
    ~logger_t() {
        // pass
    }


    virtual
    void
    emit(cocaine::logging::priorities priority,
         const std::string& message) = 0;

    template<typename... Args>
    void
    emit(cocaine::logging::priorities priority,
         const std::string& format,
         const Args&... args)
    {
        emit(priority, cocaine::format(format, args...));
    }

    virtual
    cocaine::logging::priorities
    verbosity() const = 0;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_LOGGING_HPP
