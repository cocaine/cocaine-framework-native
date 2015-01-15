#include "connection.hpp"

using namespace cocaine::framework;

connection_t::connection_t(loop_t& loop) noexcept :
    loop(loop),
    socket(loop),
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
        connection_queue.emplace_back(std::move(promise));
        state = state_t::connecting;
        // TODO: If the socket is broken it should be reinitialized under the mutex.
        socket.async_connect(
            endpoint,
            std::bind(&connection_t::on_connected, this->shared_from_this(), std::placeholders::_1)
        );
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

void connection_t::on_connected(const boost::system::error_code& ec) {
    // This callback can be called from any thread. The following mutex is guaranteed not to be
    // locked at that moment.
    std::lock_guard<std::mutex> lock(connection_queue_mutex);
    if (ec) {
        state = state_t::disconnected;
        for (auto& promise : connection_queue) {
            promise.set_exception(boost::system::system_error(ec));
        }
        connection_queue.clear();
    } else {
        state = state_t::connected;
        for (auto& promise : connection_queue) {
            promise.set_value();
        }
        connection_queue.clear();
    }
}
