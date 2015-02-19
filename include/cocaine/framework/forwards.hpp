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

template<class Session>
class basic_receiver_t;

template<class T, class Session>
class receiver;

class basic_session_t;

/// Worker side.
class worker_session_t;
class worker_t;

} // namespace framework

} // namespace cocaine
