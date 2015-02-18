#include "cocaine/framework/scheduler.hpp"

#include "cocaine/framework/detail/loop.hpp"

using namespace cocaine::framework;

void scheduler_t::operator()(closure_type fn) {
    ev.loop.post(std::move(fn));
}
