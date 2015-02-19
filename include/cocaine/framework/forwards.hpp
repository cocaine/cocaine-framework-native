#pragma once

#include "cocaine/framework/util/future.hpp"

namespace cocaine {

namespace framework {

template<typename T>
struct task {
    typedef future <T> future_type;
    typedef promise<T> promise_type;
};

/// \internal
struct event_loop_t;

class scheduler_t;

class basic_session_t;

} // namespace framework

} // namespace cocaine
