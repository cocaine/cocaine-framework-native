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
 * pointer. The reason is: in particular case the connection's event loop runs in a separate
 * thread, other in that the connection itself lives.
 * Thus no one can guarantee that all asynchronous operations are completed before the connection
 * instance be destroyed.
 */
class connection_t : public std::enable_shared_from_this<connection_t> {
    loop_t& loop;

    boost::asio::ip::tcp::socket socket;
    std::atomic<state_t> state;

    mutable std::mutex connection_queue_mutex;
    std::vector<std::shared_ptr<boost::promise<void>>> connection_queue; // TODO: s/waiting/
public:
    /*!
     * \note the event loop reference should be valid until all asynchronous operations complete
     * otherwise the behavior is undefined.
     */
    connection_t(loop_t& loop) noexcept;

    /*!
     * \note the class does passive connection monitoring, e.g. it won't be immediately notified
     * if the real connection has been lost, but after the next send/recv attempt.
     */
    bool connected() const noexcept;

    future_t<void> connect(const endpoint_t& endpoint);

private:
    void on_connected(const boost::system::error_code& ec);
};

} // namespace framework

} // namespace cocaine
