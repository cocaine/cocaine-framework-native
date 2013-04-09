#ifndef COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP
#define COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP

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

#endif // COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP
