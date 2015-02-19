#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/thread/thread.hpp>

#include <asio/local/stream_protocol.hpp>

#include <cocaine/locked_ptr.hpp>

#include "cocaine/framework/receiver.hpp"
#include "cocaine/framework/session.hpp"
#include "cocaine/framework/worker/options.hpp"
#include "cocaine/framework/worker/sender.hpp"

namespace cocaine {

namespace framework {

template<class Session>
class receiver<io::rpc_tag, Session> {
    std::shared_ptr<basic_receiver_t<Session>> d;

public:
    receiver(std::shared_ptr<basic_receiver_t<Session>> d) :
        d(d)
    {}

    typename task<boost::optional<std::string>>::future_type
    recv() {
        auto d = this->d;
        return d->recv().then([d](typename task<detail::decoder_t::message_type>::future_type& f) -> boost::optional<std::string> {
            const auto message = f.get();
            const std::uint64_t id = message.type();
            switch (id) {
            case io::event_traits<io::rpc::chunk>::id: {
                std::string chunk;
                io::type_traits<
                    typename io::event_traits<io::rpc::chunk>::argument_type
                >::unpack(message.args(), chunk);
                return chunk;
            }
            case io::event_traits<io::rpc::error>::id: {
                int id;
                std::string reason;
                io::type_traits<
                    typename io::event_traits<io::rpc::error>::argument_type
                >::unpack(message.args(), id, reason);
                throw std::runtime_error(reason);
            }
            case io::event_traits<io::rpc::choke>::id:
                return boost::none;
            default:
                COCAINE_ASSERT(false);
            }

            return boost::none;
        });
    }
};

class worker_t;
class worker_session_t;

class dispatch_t {
    typedef io::stream_of<std::string>::tag streaming_tag;

public:
    typedef sender  <io::rpc_tag, worker_session_t> sender_type;
    typedef receiver<io::rpc_tag, worker_session_t> receiver_type;
    typedef std::function<void(sender_type, receiver_type)> handler_type;

private:
    std::unordered_map<std::string, handler_type> handlers;

public:
    boost::optional<handler_type> get(const std::string& event) {
        auto it = handlers.find(event);
        if (it != handlers.end()) {
            return it->second;
        }

        return boost::none;
    }

    template<typename F>
    void on(std::string event, F handler) {
        handlers[event] = std::move(handler);
    }
};

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
    synchronized<std::unordered_map<std::uint64_t, std::shared_ptr<detail::shared_state_t>>> channels;

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

class worker_t {
    class impl;
    std::unique_ptr<impl> d;

    dispatch_t dispatch;

public:
    worker_t(options_t options);
    ~worker_t();

    template<class F>
    void on(std::string event, F handler) {
        dispatch.on(event, std::move(handler));
    }

    auto options() const -> const options_t&;

    int run();
};

} // namespace framework

} // namespace cocaine
