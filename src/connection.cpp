#include "cocaine/framework/connection.hpp"

#include "cocaine/framework/detail/log.hpp"

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

    void operator()() {
        connection->channel->writer->write(message, std::bind(&push_t::on_write, shared_from_this(), ph::_1));
    }

private:
    void on_write(const std::error_code& ec) {
        // TODO: Notify connection if any error occurred. It should disconnect all clients.
    }
};

connection_t::connection_t(loop_t& loop) noexcept :
    loop(loop),
    state(state_t::disconnected),
    counter(1)
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
            std::bind(&connection_t::on_connected, shared_from_this(), ph::_1)
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
    loop.dispatch(
        std::bind(
            &push_t::operator(),
            std::make_shared<push_t>(std::move(message), shared_from_this())
        )
    );
}

void connection_t::on_connected(const std::error_code& ec) {
    // This callback can be called from any thread. The following mutex is guaranteed not to be
    // locked at that moment.
    std::lock_guard<std::mutex> lock(connection_queue_mutex);
    BOOST_ASSERT(state_t::connecting == state);
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

        std::lock_guard<std::mutex> lock(channel_mutex);
        channel = std::make_shared<io::channel<protocol_type>>(std::move(socket));
        channel->reader->read(message, std::bind(&connection_t::on_read, shared_from_this(), ph::_1));
    }
}

void connection_t::on_read(const std::error_code& ec) {
    BH_LOG(detail::logger, detail::debug, "read event: %s", ec.message().c_str());

    auto channels = this->channels.synchronize();
    if (ec) {
        for (auto channel : *channels) {
            channel.second->error(ec);
        }
        channels->clear();
        return;
    }

    auto it = channels->find(message.span());
    if (it == channels->end()) {
        // TODO: Log that received an orphan message.
    } else {
        it->second->process(std::move(message));
        // It should check message id. If ok - put in the queue. [Overhead on copy]
        // [Maybe] If dispatch is present - try to send to it (1 time).
    }

    // TODO: Continue reading if (!ec).
}
