#pragma once

#include <memory>
#include <string>

#include <cocaine/forwards.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

namespace worker {

class sender {
    std::shared_ptr<basic_sender_t<worker_session_t>> session;

public:
    /*!
     * \note this constructor is intentionally left implicit.
     */
    sender(std::shared_ptr<basic_sender_t<worker_session_t>> session);
    ~sender();

    sender(const sender& other) = delete;
    sender(sender&&) = default;

    sender& operator=(const sender& other) = delete;
    sender& operator=(sender&&) = default;

    auto write(std::string message) -> typename task<sender>::future_type;
    auto error(int id, std::string reason) -> typename task<void>::future_type;
    auto close() -> typename task<void>::future_type;
};

} // namespace worker

} // namespace framework

} // namespace cocaine
