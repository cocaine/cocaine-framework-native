#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <cocaine/forwards.hpp>
#include <cocaine/rpc/protocol.hpp>

#include <cocaine/detail/service/node/messages.hpp>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/worker/sender.hpp"
#include "cocaine/framework/worker/receiver.hpp"

namespace cocaine {

namespace framework {

// TODO: Can be hidden.
class dispatch_t {
    // TODO: Can be hidden with transform_traits.
    typedef io::stream_of<std::string>::tag streaming_tag;

public:
    typedef sender  <io::rpc_tag, worker_session_t> sender_type;
    typedef receiver<io::rpc_tag, worker_session_t> receiver_type;
    typedef std::function<void(sender_type, receiver_type)> handler_type;

private:
    std::unordered_map<std::string, handler_type> handlers;

public:
    boost::optional<handler_type> get(const std::string& event);

    void on(std::string event, handler_type handler);
};

}

}
