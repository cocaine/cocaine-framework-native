#pragma once

#include <memory>
#include <functional>

#include <asio/ip/tcp.hpp>

#include <cocaine/common.hpp>
#include <cocaine/rpc/asio/channel.hpp>

#include "cocaine/framework/common.hpp"
#include "cocaine/framework/config.hpp"

namespace cocaine {

namespace framework {

/*!
 * \brief The basic_connection_t class
 *
 * \thread_safety unsafe.
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

public:
    basic_connection_t(loop_t& loop) :
        loop(loop),
        state(state_t::disconnected)
    {}

    basic_connection_t(basic_connection_t&& other) = default;
    basic_connection_t& operator=(basic_connection_t&& other) = default;

    bool connected() const noexcept {
        return state == state_t::connected;
    }

    void connect(const endpoint_t& endpoint, callback_type callback) {
        switch (state) {
        case state_t::disconnected: {
            std::unique_ptr<socket_type> socket(new socket_type(loop));
            socket->open(endpoint.protocol());

            channel = std::unique_ptr<channel_type>(new channel_type(std::move(socket)));
            channel->socket->async_connect(
                endpoint,
                std::bind(&basic_connection_t::on_connect, shared_from_this(), std::placeholders::_1, callback)
            );

            // The code above can throw std::bad_alloc, so here it is the right place to change
            // current object's state.
            state = state_t::connecting;
            break;
        }
        case state_t::connecting: {
            loop.dispatch(std::bind(callback, asio::error::in_progress));
            break;
        }
        case state_t::connected: {
            loop.dispatch(std::bind(callback, asio::error::already_connected));
            break;
        }
        default:
            COCAINE_ASSERT(false);
        }
    }

    void disconnect() {}

    void read(callback_type callback) {}
    void write(callback_type callback) {}

private:
    void on_connect(const std::error_code& ec, callback_type callback) {
        COCAINE_ASSERT(state == state_t::connecting);

        state = ec ? state_t::disconnected : state_t::connected;
        callback(ec);
    }
};

}

}
