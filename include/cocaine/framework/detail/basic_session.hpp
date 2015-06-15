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
#include <unordered_map>

#include <boost/asio/ip/tcp.hpp>

#include <asio/ip/tcp.hpp>

#include <cocaine/common.hpp>
#include <cocaine/locked_ptr.hpp>
#include <cocaine/rpc/asio/channel.hpp>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/receiver.hpp"

#include "cocaine/framework/detail/decoder.hpp"

namespace cocaine {

namespace framework {

/// Represents resumable session implementation.
/*!
 * \note I can't guarantee lifetime safety in other way than by making this class living as shared
 * pointer. The reason is: in particular case the connection's event loop runs in a separate
 * thread, other in that the connection itself lives.
 * Thus no one can guarantee that all asynchronous operations are completed before the connection
 * instance be destroyed.
 *
 * \internal
 * \threadsafe
 */
class basic_session_t : public std::enable_shared_from_this<basic_session_t> {
    typedef asio::ip::tcp protocol_type;
    typedef protocol_type::socket socket_type;

    typedef std::shared_ptr<basic_sender_t<basic_session_t>> basic_sender_type;

    typedef io::channel<protocol_type, io::encoder_t, detail::decoder_t> channel_type;

    typedef std::unordered_map<std::uint64_t, std::shared_ptr<shared_state_t>> channels_type;

    class push_t;

public:
    typedef boost::asio::ip::tcp::endpoint endpoint_type;

    typedef socket_type::native_handle_type native_handle_type;

    typedef std::tuple<
        basic_sender_type,
        std::shared_ptr<basic_receiver_t<basic_session_t>>
    > invoke_result;

private:
    enum class state_t {
        /// The session is in resumable disconnected state.
        disconnected = 0,
        /// The session is connecting.
        connecting,
        /// The session is connected.
        connected
    };

    scheduler_t& scheduler;
    std::atomic<bool> closed;

    std::atomic<int> state;
    std::atomic<std::uint64_t> counter;
    decoded_message message;

    synchronized<std::shared_ptr<channel_type>> transport;
    synchronized<channels_type> channels;

    std::mutex mutex;

public:
    /// Constructs a disconnected session.
    ///
    /// \warning the scheduler reference should be valid until all asynchronous operations complete
    /// otherwise the behavior is undefined.
    explicit basic_session_t(scheduler_t& scheduler) noexcept;

    ~basic_session_t();

    /// Checks whether the session is in connected state.
    ///
    /// \note the session does passive connection monitoring, e.g. it won't be immediately notified
    /// if the real connection has been lost, but after the next send/recv attempt.
    bool connected() const noexcept;

    /// \threadsafe
    future<std::error_code>
    connect(const endpoint_type& endpoint);

    /// \threadsafe
    auto connect(const std::vector<endpoint_type>& endpoints) -> task<std::error_code>::future_type;

    /*!
     * Returns the endpoint of the connected peer if the session is in connected state; otherwise
     * returns none.
     *
     * \threadsafe
     */
    auto endpoint() const -> boost::optional<endpoint_type>;

    native_handle_type
    native_handle() const;

    /// Cancels the current session, moving it to the disconnected unrecoverable state.
    ///
    /// \warning the session becomes invalid after this call, its further external usage will
    /// results in undefined behavior.
    void
    cancel();

    /*!
     * Sends an invocation event and creates a new channel accociated with it.
     *
     * \note if the future returned throws an exception that means that the data will never be
     * received, but if it doesn't - the data is not guaranteed to be received. It is possible for
     * the other end of connection to hang up immediately after the future returns ok.
     *
     * If you send a **mute** event, there is no way to obtain guarantees of successful message
     * transporting.
     *
     * \threadsafe
     */
    auto invoke(std::function<io::encoder_t::message_type(std::uint64_t)> encoder)
        -> task<invoke_result>::future_type;

    /// TODO: Implement: invoke_mute - sends an invoke event without channel creation.

    /// Sends an event without creating a new channel.
    future<void>
    push(io::encoder_t::message_type&& message);

    /*!
     * Unsubscribes a channel with the given span.
     *
     * \todo return operation result (revoked/notfound).
     */
    void revoke(std::uint64_t span);

private:
    /// Called on socket connect event.
    void
    on_connect(const std::error_code& ec, promise<std::error_code> pr, std::unique_ptr<socket_type>& socket);

    /// Called on socket read event.
    void
    on_read(const std::error_code& ec);

    /// Called on socket error while handling read or write event.
    void
    on_error(const std::error_code& ec);

    void
    pull(std::shared_ptr<channel_type> transport);
};

}

}
