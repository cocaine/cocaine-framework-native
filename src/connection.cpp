#include "cocaine/framework/connection.hpp"

namespace ph = std::placeholders;

using namespace cocaine::framework;

basic_connection_t::basic_connection_t(loop_t& loop) :
    loop(loop),
    state(state_t::disconnected),
    counter(0)
{}

bool basic_connection_t::connected() const {
    return state == state_t::connected;
}

void basic_connection_t::connect(const endpoint_t& endpoint, callback_type callback) {
    switch (state) {
    case state_t::disconnected: {
        std::unique_ptr<socket_type> socket(new socket_type(loop));
        socket->open(endpoint.protocol());

        channel.reset(new channel_type(std::move(socket)));
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

void basic_connection_t::disconnect() {
    channel.reset();
    loop.post(std::bind(&basic_connection_t::cancel, shared_from_this()));
}

void basic_connection_t::read(io::decoder_t::message_type& message, callback_type callback) {
    if (channel) {
        auto current = counter++;
        pending[current] = callback;

        channel->reader->read(
            message,
            std::bind(&basic_connection_t::on_readwrite, shared_from_this(), ph::_1, current)
        );
    } else {
        loop.post(std::bind(callback, io_provider::error::not_connected));
    }
}

void basic_connection_t::write(const io::encoder_t::message_type& message, callback_type callback) {
    if (channel) {
        auto current = counter++;
        pending[current] = callback;

        channel->writer->write(
            message,
            std::bind(&basic_connection_t::on_readwrite, shared_from_this(), ph::_1, current)
        );
    } else {
        loop.post(std::bind(callback, io_provider::error::not_connected));
    }
}

void basic_connection_t::cancel() {
    state = state_t::disconnected;

    for (auto it = pending.begin(); it != pending.end(); ++it) {
        it->second(io_provider::error::operation_aborted);
    }

    pending.clear();
}

void basic_connection_t::on_connect(const std::error_code& ec, callback_type callback) {
    COCAINE_ASSERT(state == state_t::connecting);

    if (ec) {
        state = state_t::disconnected;
        channel.reset();
    } else {
        state = state_t::connected;
    }

    callback(ec);
}

void basic_connection_t::on_readwrite(const std::error_code& ec, std::uint64_t token) {
    if (ec) {
        state = state_t::disconnected;
        channel.reset();
    }

    auto it = pending.find(token);
    COCAINE_ASSERT(it != pending.end());

    pending.erase(it);

    it->second(ec);
}
