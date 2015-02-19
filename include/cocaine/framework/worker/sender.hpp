#pragma once

#include <memory>
#include <string>

#include <cocaine/forwards.hpp>
// TODO: WTF? Detail???
#include <cocaine/detail/service/node/messages.hpp>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/sender.hpp"

namespace cocaine {

namespace framework {

template<>
class sender<io::rpc_tag, worker_session_t> {
    std::shared_ptr<basic_sender_t<worker_session_t>> session;

public:
    sender(std::shared_ptr<basic_sender_t<worker_session_t>> session);
    ~sender();

    sender(const sender& other) = delete;
    sender(sender&& other) = default;

    sender& operator=(const sender& other) = delete;
    sender& operator=(sender&& other) = default;

    auto write(std::string message) -> typename task<sender>::future_type;
    auto error(int id, std::string reason) -> typename task<void>::future_type;
    auto close() -> typename task<void>::future_type;
};

} // namespace framework

} // namespace cocaine
