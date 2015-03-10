#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <cocaine/forwards.hpp>
#include <cocaine/rpc/protocol.hpp>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/worker/sender.hpp"
#include "cocaine/framework/worker/receiver.hpp"

namespace cocaine {

namespace framework {

// TODO: Can be hidden.
class dispatch_t {
public:
    typedef std::function<void(worker::sender, worker::receiver)> handler_type;

private:
    std::unordered_map<std::string, handler_type> handlers;

public:
    boost::optional<handler_type> get(const std::string& event);

    void on(std::string event, handler_type handler);
};

}

}
