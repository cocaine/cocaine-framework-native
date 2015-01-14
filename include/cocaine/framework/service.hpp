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

    boost::future<void> connect(const endpoint_t& endpoint) {
        std::shared_ptr<boost::promise<void>> promise(new boost::promise<void>());
        boost::future<void> future = promise->get_future();

        // If the socket is broken it should be reinitialized under the mutex.
        socket.async_connect(endpoint, [this, promise](const boost::system::error_code& ec){
            // This callback can be called from any thread.
            if (ec) {
                promise->set_exception(boost::system::system_error(ec));
            } else {
                state = state_t::connected;
                promise->set_value();
            }
        });
        return future;
    }

    bool connected() const noexcept {
        return state == state_t::connected;
    }
};

} // namespace framework

} // namespace cocaine
