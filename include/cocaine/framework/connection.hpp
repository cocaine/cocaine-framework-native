#pragma once

#include <memory>
#include <functional>
#include <unordered_map>

#include <asio/ip/tcp.hpp>

#include <cocaine/common.hpp>
#include <cocaine/rpc/asio/channel.hpp>

#include "cocaine/framework/config.hpp"

namespace cocaine {

namespace framework {

/*!
 * \brief The basic_connection_t class
 *
 * \thread_safety unsafe - all methods of this class **must** be called from the event loop thread,
 * otherwise the behavior is undefined.
 *
 * \internal
 */
class basic_connection_t : public std::enable_shared_from_this<basic_connection_t> {
    typedef std::function<void(const std::error_code&)> callback_type;

public:
    typedef asio::ip::tcp protocol_type;
    typedef protocol_type::socket socket_type;
    typedef io::channel<protocol_type> channel_type;

    enum class state_t {
        disconnected,
        connecting,
        connected
    };

    loop_t& loop;
    state_t state;
    std::unique_ptr<channel_type> channel;

    // This map represents active callbacks holder. There is a better way to achieve the same
    // functionality - call user callbacks even if the operation is aborted, but who cares.
    std::uint64_t counter;
    std::unordered_map<std::uint64_t, callback_type> pending;

public:
    basic_connection_t(loop_t& loop);

    basic_connection_t(basic_connection_t&& other) = default;
    basic_connection_t& operator=(basic_connection_t&& other) = default;

    bool connected() const;
    void connect(const endpoint_t& endpoint, callback_type callback);
    void disconnect();

    void read(io::decoder_t::message_type& message, callback_type callback);
    void write(const io::encoder_t::message_type& message, callback_type callback);

private:
    void cancel();
    void on_connect(const std::error_code& ec, callback_type callback);
    void on_readwrite(const std::error_code& ec, std::uint64_t token);
};

}

}
