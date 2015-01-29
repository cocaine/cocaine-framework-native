#pragma once

#include <cstdint>
#include <memory>

#include <cocaine/rpc/asio/encoder.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

// app::enqueue - allows [write | error | close]
//   write - -//-,
//   error - void,
//   close - void.

class basic_session_t;

class basic_sender_t {
    std::uint64_t id;
    std::shared_ptr<basic_session_t> connection;

public:
    basic_sender_t(std::uint64_t id, std::shared_ptr<basic_session_t> connection);

    /*!
     * \todo \throw encoding_error if failed to encode the arguments given.
     * \todo \throw std::system_error if sender is in invalid state, for example after connection
     * lose.
     */
    template<class Event, class... Args>
    auto
    send(Args&&... args) -> future_t<void> {
        return send(io::encoded<Event>(id, std::forward<Args>(args)...));
    }

    auto send(io::encoder_t::message_type&& message) -> future_t<void>;
};

} // namespace framework

} // namespace cocaine
