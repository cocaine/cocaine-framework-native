#pragma once

#include "cocaine/framework/util/future.hpp"

namespace cocaine {

namespace framework {

template<typename T>
using promise_t = promise<T>;

template<typename T>
using future_t = future<T>;

} // namespace framework

} // namespace cocaine
