#pragma once

#define CF_USE_INTERNAL_LOGGING

#ifdef CF_USE_INTERNAL_LOGGING

#include <boost/preprocessor/slot/counter.hpp>

#include <blackhole/logger.hpp>
#include <blackhole/logger/wrapper.hpp>
#include <blackhole/macro.hpp>
#include <blackhole/scoped_attributes.hpp>
#include <blackhole/utils/format.hpp>

namespace cocaine {

namespace framework {

namespace detail {

enum level_t {
    debug,
    notice,
    info,
    warn,
    error
};

typedef blackhole::verbose_logger_t<level_t> logger_type;
typedef blackhole::wrapper_t<logger_type> wrapper_type;

logger_type create();

static logger_type logger = create();

} // namespace detail

} // namespace framework

} // namespace cocaine

#   define CF_EC(ec) ec ? ec.message().c_str() : "ok"
#   define CF_LOG BH_LOG
#   define CF_DBG(...) CF_LOG(detail::logger, detail::debug, __VA_ARGS__)

static inline
std::string merge_context(std::string context) {
    auto record = cocaine::framework::detail::logger.open_record(cocaine::framework::detail::error);
    if (record.valid()) {
        if (auto current = record.attributes().find("context")) {
            return blackhole::utils::format("%s/%s", boost::get<std::string>(current->value), context);
        }
    }
    return context;
}

#define CF_CTX(...) \
    ::blackhole::scoped_attributes_t BOOST_PP_CAT(__context__, __COUNTER__)( \
        ::cocaine::framework::detail::logger, \
        ::blackhole::attribute::set_t({ \
            { "context", merge_context(::blackhole::utils::format(__VA_ARGS__)) } \
        }) \
    );

#else
#   define CF_EC(...)
#   define CF_LOG(...)
#   define CF_DBG(...)
#   define CF_CTX(...)
#endif
