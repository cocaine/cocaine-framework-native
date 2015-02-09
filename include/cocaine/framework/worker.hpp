#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <asio/local/stream_protocol.hpp>

#include <cocaine/common.hpp>
#include <cocaine/idl/node.hpp>
#include <cocaine/rpc/asio/channel.hpp>

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

class worker_t {
public:
    typedef io::stream_of<std::string>::tag streaming_tag;

    typedef sender<streaming_tag> sender_type;
    typedef receiver<streaming_tag> receiver_type;

    typedef std::function<void(sender_type, receiver_type)> handler_type;

    options_t options;

    typedef asio::local::stream_protocol protocol_type;
    typedef protocol_type::socket socket_type;
    typedef io::channel<protocol_type> channel_type;

    asio::io_service loop;
    asio::deadline_timer heartbeat_timer;
    asio::deadline_timer disown_timer;

    io::decoder_t::message_type message;
    std::unique_ptr<channel_type> channel;

    std::unordered_map<std::string, handler_type> handlers;

public:
    worker_t(options_t options);

    template<class F>
    void on(std::string event, F handler) {
        handlers[event] = handler;
    }

    int run();

private:
    void stop();
    void dispatch(const io::decoder_t::message_type& message);

    void on_read(const std::error_code& ec);
    void on_write(const std::error_code& ec);

    void send_heartbeat(const std::error_code& ec);
    void on_heartbeat_sent(const std::error_code& ec);

    void on_disown(const std::error_code& ec);
    void on_heartbeat();
};

} // namespace framework

} // namespace cocaine
