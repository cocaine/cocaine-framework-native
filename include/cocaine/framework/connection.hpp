#pragma once

#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>

#include <cocaine/common.hpp>
#include <cocaine/rpc/asio/channel.hpp>

#include "cocaine/framework/util/future.hpp"

namespace cocaine {

namespace framework {

using loop_t = asio::io_service;
using endpoint_t = asio::ip::tcp::endpoint;

template<typename T>
using promise_t = promise<T>;

template<typename T>
using future_t = future<T>;

enum class state_t {
    disconnected,
    connecting,
    connected
};

// TODO: It's a cocaine session actually, may be rename?
/*!
 * \note I can't guarantee lifetime safety in other way than by making this class living as shared
 * pointer. The reason is: in particular case the connection's event loop runs in a separate
 * thread, other in that the connection itself lives.
 * Thus no one can guarantee that all asynchronous operations are completed before the connection
 * instance be destroyed.
 */
class connection_t : public std::enable_shared_from_this<connection_t> {
    typedef asio::ip::tcp protocol_type;
    typedef protocol_type::socket socket_type;
    typedef io::channel<protocol_type> channel_type;

    loop_t& loop;

    std::atomic<state_t> state;
    std::unique_ptr<socket_type> socket;

    mutable std::mutex connection_queue_mutex;
    std::vector<promise_t<void>> connection_queue;

    std::atomic<std::uint64_t> counter;
    std::shared_ptr<channel_type> channel;

    mutable std::mutex channel_mutex;

    class push_t;

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

    template<class T, class... Args>
    void invoke(Args&&... args);

    void invoke(io::encoder_t::message_type&& message);

    loop_t& io() const noexcept;

private:
    void on_connected(const std::error_code& ec);
};

template<class T, class... Args>
void connection_t::invoke(Args&&... args) {
    return invoke(io::encoded<T>(counter + 1, std::forward<Args>(args)...));
}

} // namespace framework

} // namespace cocaine
