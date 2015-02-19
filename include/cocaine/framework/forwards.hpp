#pragma once

#include "cocaine/framework/util/future.hpp"

namespace cocaine {

namespace framework {

template<typename T>
using promise_t = promise<T>;

template<typename T>
using future_t = future<T>;

template<typename T>
using promise_type = promise<T>;

template<typename T>
using future_type = future<T>;

/// \internal
struct event_loop_t;

class scheduler_t;

} // namespace framework

} // namespace cocaine
