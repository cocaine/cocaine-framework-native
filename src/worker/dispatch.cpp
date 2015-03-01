#include "cocaine/framework/worker/dispatch.hpp"

using namespace cocaine::framework;

boost::optional<dispatch_t::handler_type> dispatch_t::get(const std::string& event) {
    auto it = handlers.find(event);
    if (it != handlers.end()) {
        return it->second;
    }

    return boost::none;
}

void dispatch_t::on(std::string event, dispatch_t::handler_type handler) {
    handlers[event] = std::move(handler);
}
