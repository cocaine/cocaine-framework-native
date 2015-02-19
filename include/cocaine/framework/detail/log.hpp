#pragma once

#include "cocaine/framework/config.hpp"

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

logger_type& logger();

std::string merge_context(std::string context);

} // namespace detail

} // namespace framework

} // namespace cocaine

/// Silently cast std::uint64_t to unsigned long long to suppress logger format warnings
/// in cross-platform manner.
#define CF_US(sized) static_cast<unsigned long long>(sized)

#   define CF_EC(ec) ec ? ec.message().c_str() : "ok"
#   define CF_LOG BH_LOG
#   define CF_DBG(...) CF_LOG(::cocaine::framework::detail::logger(), ::cocaine::framework::detail::debug, __VA_ARGS__)

#define CF_CTX(...) \
    ::blackhole::scoped_attributes_t BOOST_PP_CAT(__context__, __COUNTER__)( \
        ::cocaine::framework::detail::logger(), \
        ::blackhole::attribute::set_t({ \
            { "context", ::cocaine::framework::detail::merge_context(::blackhole::utils::format(__VA_ARGS__)) } \
        }) \
    );

#else

#define CF_US(...)
#   define CF_EC(...)
#   define CF_LOG(...)
#   define CF_DBG(...)
#   define CF_CTX(...)
#endif
