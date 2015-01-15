#pragma once

#include <string>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

namespace cocaine {

namespace framework {

using loop_t = boost::asio::io_service;
using endpoint_t = boost::asio::ip::tcp::endpoint;

template<typename T>
using promise_t = boost::promise<T>;

template<typename T>
using future_t = boost::future<T>;

enum class state_t {
    disconnected,
    connecting,
    connected
};

/*!
 * \note I can't guarantee lifetime safety in other way than by making this class living as shared
 * pointer. The reason is: in particular case the service's event loop runs in a separate thread,
 * other in that the service lives.
 * Thus no one can guarantee that all asynchronous operations are completed before the service
 * instance be destroyed.
 */
template<class T>
class low_level_service : public std::enable_shared_from_this<low_level_service<T>> {
    // TODO: Static Assert - T contains protocol tag.

    loop_t& loop;

    boost::asio::ip::tcp::socket socket;
    std::atomic<state_t> state;

    mutable std::mutex connection_queue_mutex;
    std::vector<std::shared_ptr<boost::promise<void>>> connection_queue;
public:
    /*!
     * \note the event loop reference should be valid until all asynchronous operations complete
     * otherwise the behavior is undefined.
     */
    low_level_service(std::string name, loop_t& loop) noexcept :
        loop(loop),
        socket(loop),
        state(state_t::disconnected)
    {}

    future_t<void> connect(const endpoint_t& endpoint) {
        auto promise = std::make_shared<promise_t<void>>();

        std::lock_guard<std::mutex> lock(connection_queue_mutex);
        switch (state.load()) {
        case state_t::disconnected: {
            connection_queue.push_back(promise);
            state = state_t::connecting;
            // TODO: If the socket is broken it should be reinitialized under the mutex.
            socket.async_connect(endpoint, std::bind(&low_level_service::on_connected, this->shared_from_this(), std::placeholders::_1));
            break;
        }
        case state_t::connecting: {
            connection_queue.push_back(promise);
            break;
        }
        case state_t::connected: {
             BOOST_ASSERT(connection_queue.empty());
             promise->set_value();
            break;
        }
        default:
            BOOST_ASSERT(false);
        }

        return promise->get_future();
    }

    /*!
     * \note the service does passive connection monitoring, e.g. it won't be immediately notified
     * if the real connection has been lost, but after the next send/recv attempt.
     */
    bool connected() const noexcept {
        return state == state_t::connected;
    }

private:
    void on_connected(const boost::system::error_code& ec) {
        // This callback can be called from any thread. The following mutex is guaranteed not to be
        // locked at that moment.
        std::lock_guard<std::mutex> lock(connection_queue_mutex);
        if (ec) {
            state = state_t::disconnected;
            for (auto promise : connection_queue) {
                promise->set_exception(boost::system::system_error(ec));
            }
            connection_queue.clear();
        } else {
            state = state_t::connected;
            for (auto promise : connection_queue) {
                promise->set_value();
            }
            connection_queue.clear();
        }
    }
};

} // namespace framework

} // namespace cocaine
