#pragma once

#define CF_USE_INTERNAL_LOGGING

#ifdef CF_USE_INTERNAL_LOGGING

#include <blackhole/logger.hpp>
#include <blackhole/macro.hpp>
#include <blackhole/scoped_attributes.hpp>
#include <blackhole/utils/format.hpp>

namespace cocaine {

namespace framework {

namespace detail {

enum level_t {
    debug,
    info,
    warn,
    error
};

blackhole::verbose_logger_t<cocaine::framework::detail::level_t> create();

static blackhole::verbose_logger_t<level_t> logger = create();

} // namespace detail

} // namespace framework

} // namespace cocaine

#   define CF_EC(ec) ec ? ec.message().c_str() : "ok"
#   define CF_LOG BH_LOG
#   define CF_DBG(...) CF_LOG(detail::logger, detail::debug, __VA_ARGS__)
#   define CF_CTX(...) \
    blackhole::scoped_attributes_t __context__(cocaine::framework::detail::logger, blackhole::attribute::set_t({{ "context", blackhole::utils::format(__VA_ARGS__) }}));
#else
#   define CF_EC(...)
#   define CF_LOG(...)
#   define CF_DBG(...)
#   define CF_CTX(...)
#endif
