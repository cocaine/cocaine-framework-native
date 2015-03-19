/*
    Copyright (c) 2015 Evgeny Safronov <division494@gmail.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.
    This file is part of Cocaine.
    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

#include <asio/local/stream_protocol.hpp>

#include <cocaine/forwards.hpp>
#include <cocaine/locked_ptr.hpp>

#include <cocaine/detail/service/node/messages.hpp>

#include "cocaine/framework/message.hpp"
#include "cocaine/framework/worker/dispatch.hpp"

#include "cocaine/framework/detail/channel.hpp"
#include "cocaine/framework/detail/decoder.hpp"

namespace cocaine {

namespace framework {

/*!
 * \brief The worker_session_t class - implementation heart of any worker.
 *
 * \internal
 */
class worker_session_t : public std::enable_shared_from_this<worker_session_t> {
    template<class>
    class push_t;

public:
    typedef asio::local::stream_protocol protocol_type;
    typedef detail::channel<protocol_type, io::encoder_t, detail::decoder_t> channel_type;

    typedef std::function<void(worker::sender, worker::receiver)> handler_type;

private:
    dispatch_t& dispatch;
    scheduler_t& scheduler;
    executor_t executor;

    detail::decoder_t::message_type message;
    std::unique_ptr<channel_type> channel;
    synchronized<std::unordered_map<std::uint64_t, std::shared_ptr<shared_state_t>>> channels;

    // Health.
    asio::deadline_timer heartbeat_timer;
    asio::deadline_timer disown_timer;

public:
    worker_session_t(dispatch_t& dispatch, scheduler_t& scheduler, executor_t executor);

    void connect(std::string endpoint, std::string uuid);

    auto push(std::uint64_t span, io::encoder_t::message_type&& message) -> typename task<void>::future_type;
    auto push(io::encoder_t::message_type&& message) -> typename task<void>::future_type;
    void revoke(std::uint64_t span);

private:
    /// Handle incoming protocol message.
    void on_read(const std::error_code& ec);

    /// Notifies all channels about worker fatal error, after which a normal execution cannot be
    /// guaranteed.
    ///
    /// Usually this callback is called on write/read failure or on disown timeout expiration.
    void on_error(const std::error_code& ec);

    /// Notifies all channels about disowning and stops the worker.
    void on_disown(const std::error_code& ec);

    /// Sends a handshake protocol message to the runtime.
    void handshake(const std::string& uuid);

    /// Sends a terminate protocol message to the runtime, then stops the event loop.
    void terminate(io::rpc::terminate::code code, std::string reason);

    /// Handles terminate event completion (i.e performs graceful shutdown).
    void on_terminate(task<void>::future_move_type);

    /// Resets the disown timer. Usually called after receiving a heartbeat event from the runtime.
    void inhale();

    /// Sends a heartbeat message to the runtime, then restarts the heartbeat timer.
    /// Usually called via timer, except the first heartbeat, which is send manually.
    void exhale(const std::error_code& ec = std::error_code());

    void process();
    void process_handshake();
    void process_heartbeat();
    void process_terminate();
    void process_invoke();
    void process_chunk();
    void process_error();
    void process_choke();
};

}

}
