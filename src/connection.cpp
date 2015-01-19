#include "connection.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

class connection_t::push_t : public std::enable_shared_from_this<push_t> {
    io::encoder_t::message_type message;
    std::shared_ptr<connection_t> connection;

public:
    explicit push_t(io::encoder_t::message_type&& message, std::shared_ptr<connection_t> connection) :
        message(std::move(message)),
        connection(connection)
    {}

    ~push_t() {
        std::cout << "~push_t()" << std::endl;
    }

    void operator()() {
        sleep(1);
        std::cout << "write" << std::endl;
        connection->channel->writer->write(message, std::bind(&push_t::on_write, shared_from_this(), ph::_1));
    }

private:
    void on_write(const std::error_code& ec) {
        std::cout << ec.message() << std::endl;
        // TODO: Notify connection if any error occurred. It should disconnect all clients.
    }
};

connection_t::connection_t(loop_t& loop) noexcept :
    loop(loop),
    state(state_t::disconnected)
{}

bool connection_t::connected() const noexcept {
    return state == state_t::connected;
}

future_t<void> connection_t::connect(const endpoint_t& endpoint) {
    promise_t<void> promise;
    auto future = promise.get_future();

    std::lock_guard<std::mutex> lock(connection_queue_mutex);
    switch (state.load()) {
    case state_t::disconnected: {
        socket = std::make_unique<socket_type>(loop);
        socket->async_connect(
            endpoint,
            std::bind(&connection_t::on_connected, this->shared_from_this(), ph::_1)
        );

        // The code above can throw std::bad_alloc, so here it is the right place to change
        // current object's state.
        connection_queue.emplace_back(std::move(promise));
        state = state_t::connecting;
        break;
    }
    case state_t::connecting: {
        connection_queue.emplace_back(std::move(promise));
        break;
    }
    case state_t::connected: {
         BOOST_ASSERT(connection_queue.empty());
         promise.set_value();
        break;
    }
    default:
        BOOST_ASSERT(false);
    }

    return future;
}

void connection_t::invoke(io::encoder_t::message_type&& message) {
    // Increase counter atomically.
    counter++;

    // Create and insert channel [lock].
    // TODO: Lock.
    // TODO: Check if the channel is already exists.
    // TODO: Unlock.

    // Make handler, that should live until the message completely sent.

    std::lock_guard<std::mutex> lock(channel_mutex);
    loop.dispatch(
        std::bind(
            &push_t::operator(),
            std::make_shared<push_t>(std::move(message), shared_from_this())
        )
    );

    // Register read callback.
    // Write message.
    // Return tx [channel handler, or upstream].
}

loop_t& connection_t::io() const noexcept {
    return loop;
}

void connection_t::on_connected(const std::error_code& ec) {
    // This callback can be called from any thread. The following mutex is guaranteed not to be
    // locked at that moment.
    std::lock_guard<std::mutex> lock(connection_queue_mutex);
    if (ec) {
        state = state_t::disconnected;
        for (auto& promise : connection_queue) {
            promise.set_exception(std::system_error(ec));
        }
        connection_queue.clear();
    } else {
        state = state_t::connected;
        for (auto& promise : connection_queue) {
            promise.set_value();
        }
        connection_queue.clear();
        channel = std::make_shared<io::channel<protocol_type>>(std::move(socket));
    }
}
