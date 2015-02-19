#pragma once

#include <memory>
#include <string>

#include <cocaine/forwards.hpp>
// TODO: WTF? Detail???
#include <cocaine/detail/service/node/messages.hpp>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/receiver.hpp"

namespace cocaine {

namespace framework {

template<>
class receiver<io::rpc_tag, worker_session_t> {
    std::shared_ptr<basic_receiver_t<worker_session_t>> session;

public:
    receiver(std::shared_ptr<basic_receiver_t<worker_session_t>> session);

    auto recv() -> typename task<boost::optional<std::string>>::future_type;
};

} // namespace framework

} // namespace cocaine
