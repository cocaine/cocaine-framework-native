#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

#include <asio/local/stream_protocol.hpp>

#include <cocaine/locked_ptr.hpp>

#include "cocaine/framework/worker/dispatch.hpp"

#include "cocaine/framework/detail/channel.hpp"

namespace cocaine {

namespace framework {

//namespace detail {

//namespace worker {

/*!
 * \brief The worker_session_t class - implementation heart of any worker.
 *
 * \internal
 */
class worker_session_t : public std::enable_shared_from_this<worker_session_t> {
    template<class>
    class push_t;

public:
    typedef dispatch_t::sender_type sender_type;
    typedef dispatch_t::receiver_type receiver_type;

    typedef asio::local::stream_protocol protocol_type;
    typedef detail::channel<protocol_type, io::encoder_t, detail::decoder_t> channel_type;

    typedef std::function<void(sender_type, receiver_type)> handler_type;

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
    void on_read(const std::error_code& ec);
    void on_error(const std::error_code& ec);

    void on_disown(const std::error_code& ec);

    void handshake(const std::string& uuid);
    void terminate(io::rpc::terminate::code code, std::string reason);

    // Called after receiving a heartbeat event from the runtime. Reset disown timer.
    void inhale();

    // Called on timer, send heartbeat to the runtime, restart the heartbeat timer.
    void exhale(const std::error_code& ec = std::error_code());

    void process();
    void on_handshake();
    void on_heartbeat();
};

//}

//}

}

}
