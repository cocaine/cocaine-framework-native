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

template<class T>
class low_level_service {
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
            // TODO: This class should be valid after callback is completed.
            socket.async_connect(endpoint, [this](const boost::system::error_code& ec){
                // This callback can be called from any thread. The following mutex is guaranteed
                // not to be locked at that moment.
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
            });
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
};

} // namespace framework

} // namespace cocaine
