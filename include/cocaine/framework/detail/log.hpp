#pragma once

#define CF_USE_INTERNAL_LOGGING

#ifdef CF_USE_INTERNAL_LOGGING

#include <blackhole/logger.hpp>
#include <blackhole/macro.hpp>

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

static const blackhole::verbose_logger_t<level_t> logger = create();

}

}

}

#define CF_LOG BH_LOG
#define CF_DBG(...) BH_LOG(detail::logger, detail::debug, __VA_ARGS__)
#else
#define CF_LOG(...)
#endif
