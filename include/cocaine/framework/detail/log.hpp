#pragma once

#include <blackhole/logger.hpp>

namespace cocaine {

namespace framework {

namespace detail {

enum class level_t {
    debug,
    info,
    warn,
    error
};

static blackhole::verbose_logger_t<level_t> logger();

}

}

}
