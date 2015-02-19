#pragma once

#include "cocaine/framework/detail/forwards.hpp"

namespace cocaine {

namespace framework {

/// \internal
struct event_loop_t {
    detail::loop_t& loop;

    explicit event_loop_t(detail::loop_t& loop) : loop(loop) {}
};

}

}
