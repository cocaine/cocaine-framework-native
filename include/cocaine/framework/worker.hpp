#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <asio/local/stream_protocol.hpp>

#include <cocaine/common.hpp>
#include <cocaine/idl/node.hpp>
#include <cocaine/rpc/asio/channel.hpp>
#include <cocaine/locked_ptr.hpp>

#include "cocaine/framework/sender.hpp"
#include "cocaine/framework/receiver.hpp"

namespace cocaine {

namespace framework {

struct options_t {
    std::string name;
    std::string uuid;
    std::string endpoint;
    std::string locator;

    options_t(int argc, char** argv);
};

class worker_t;

// TODO: Merge this one and the basic_session_t class.
class worker_session_t : public std::enable_shared_from_this<worker_session_t> {
public:
    typedef io::stream_of<std::string>::tag streaming_tag;

    typedef sender<streaming_tag, worker_session_t> sender_type;
    typedef receiver<streaming_tag> receiver_type;

    typedef asio::local::stream_protocol protocol_type;
    typedef protocol_type::socket socket_type;
    typedef io::channel<protocol_type> channel_type;

    template<class>
    class push_t;
private:
    loop_t& loop;
    io::decoder_t::message_type message;
    std::unique_ptr<channel_type> channel;
    worker_t* worker;
    synchronized<std::unordered_map<std::uint64_t, std::shared_ptr<detail::shared_state_t>>> channels;

public:
    worker_session_t(loop_t& loop, worker_t* w) : loop(loop), worker(w) {}

    void connect(std::string endpoint, std::string uuid);

    auto push(std::uint64_t span, io::encoder_t::message_type&& message) -> future_t<void>;
    auto push(io::encoder_t::message_type&& message) -> future_t<void>;
    void revoke(std::uint64_t span);

private:
    void dispatch(const io::decoder_t::message_type& message);

    void on_error(const std::error_code& ec);
    void on_read(const std::error_code& ec);
    void on_write(const std::error_code& ec);
};

class worker_t {
public:
    typedef std::function<void(worker_session_t::sender_type/*, worker_session_t::receiver_type*/)> handler_type;

    // Worker
    options_t options;
    std::unordered_map<std::string, handler_type> handlers;

    // Io.
    asio::io_service loop;

    // Health.
    asio::deadline_timer heartbeat_timer;
    asio::deadline_timer disown_timer;

    std::shared_ptr<worker_session_t> session;

public:
    worker_t(options_t options);

    template<class F>
    void on(std::string event, F handler) {
        handlers[event] = handler;
    }

    int run();

//private:
    void stop();

    // Health.
    void send_heartbeat(const std::error_code& ec);
    void on_heartbeat_sent(const std::error_code& ec);
    void on_disown(const std::error_code& ec);
    void on_heartbeat();
};

} // namespace framework

} // namespace cocaine
