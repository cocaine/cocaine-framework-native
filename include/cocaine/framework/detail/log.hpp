#pragma once

#include <blackhole/logger.hpp>
#include <blackhole/macro.hpp>

#define CF_LOG(...)

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
