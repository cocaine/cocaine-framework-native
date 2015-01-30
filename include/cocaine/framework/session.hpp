#pragma once

#include <cstdint>
#include <queue>
#include <unordered_map>

#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>

#include <cocaine/common.hpp>
#include <cocaine/locked_ptr.hpp>
#include <cocaine/rpc/asio/channel.hpp>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/common.hpp"
#include "cocaine/framework/config.hpp"
#include "cocaine/framework/receiver.hpp"
#include "cocaine/framework/sender.hpp"
#include <iostream>

/// \note temporary for debugging purposes.
template<typename T> struct deduced_type;

namespace std {

template<>
struct hash<asio::ip::tcp::endpoint> {
    typedef asio::ip::tcp::endpoint argument_type;
    typedef std::size_t value_type;

    value_type operator()(argument_type const& value) const {
        value_type const h1(std::hash<std::string>()(value.address().to_string()));
        value_type const h2(std::hash<std::uint16_t>()(value.port()));
        return h1 ^ (h2 << 1);
    }
};

} // namespace std

namespace cocaine {

namespace framework {

class basic_channel_t {
public:
    virtual ~basic_channel_t() {}

    virtual void process(io::decoder_t::message_type&& message) = 0;
    virtual void error(const std::error_code& ec) = 0;
};

template<class Event>
class channel_t : public basic_channel_t {
public:
    // TODO: May be weak!
    std::shared_ptr<basic_sender_t> tx;
    std::shared_ptr<basic_receiver_t<Event>> rx;

    channel_t(std::shared_ptr<basic_sender_t> tx) :
        tx(tx),
        rx(new basic_receiver_t<Event>())
    {}

    void process(io::decoder_t::message_type&& message) {
        rx->push(std::move(message));
    }

    void error(const std::error_code& ec) {
        rx->error(ec);
    }
};

/*!
 * \note I can't guarantee lifetime safety in other way than by making this class living as shared
 * pointer. The reason is: in particular case the connection's event loop runs in a separate
 * thread, other in that the connection itself lives.
 * Thus no one can guarantee that all asynchronous operations are completed before the connection
 * instance be destroyed.
 *
 * \thread_safety safe.
 */
class basic_session_t : public std::enable_shared_from_this<basic_session_t> {
    typedef asio::ip::tcp protocol_type;
    typedef protocol_type::socket socket_type;
    typedef io::channel<protocol_type> channel_type;

    typedef std::function<void(std::error_code)> callback_type;

    enum class state_t {
        disconnected,
        connecting,
        connected
    };

    loop_t& loop;

    std::unique_ptr<channel_type> channel;

    // This map represents active callbacks holder. There is a better way to achieve the same
    // functionality - call user callbacks even if the operation is aborted, but who cares.
    // std::uint64_t pcounter;
    // std::unordered_map<std::uint64_t, callback_type> pending;

    mutable std::mutex mutex;
    std::atomic<state_t> state;

    std::atomic<std::uint64_t> counter;

    io::decoder_t::message_type message;
    synchronized<std::unordered_map<std::uint64_t, std::shared_ptr<basic_channel_t>>> channels;

    class push_t;
public:
    /*!
     * \note the event loop reference should be valid until all asynchronous operations complete
     * otherwise the behavior is undefined.
     */
    basic_session_t(loop_t& loop) noexcept;

    /*!
     * \note the class does passive connection monitoring, e.g. it won't be immediately notified
     * if the real connection has been lost, but after the next send/recv attempt.
     */
    bool connected() const noexcept;

    auto connect(const endpoint_t& endpoint) -> future_t<std::error_code>;

    /*!
     * \brief Emits a disconnection request to the current session.
     *
     * All pending requests should result in error with the given error code.
     */
    void disconnect(const std::error_code& ec);

    /*!
     * \note if the future returned throws an exception that means that the data will never be
     * received, but if it doesn't - the data is not guaranteed to be received. It is possible for
     * the other end of connection to hang up immediately after the future returns ok.
     *
     * If you send a **mute** event, there is no way to obtain guarantees of successful message
     * transporting.
     */
    template<class Event, class... Args>
    future_t<std::tuple<std::shared_ptr<basic_sender_t>, std::shared_ptr<basic_receiver_t<Event>>>>
    invoke(Args&&... args) {
        const auto id = counter++;
        auto message = io::encoded<Event>(id, std::forward<Args>(args)...);
        auto tx = std::make_shared<basic_sender_t>(id, shared_from_this());
        auto channel = std::make_shared<channel_t<Event>>(tx);

        // TODO: Do not insert mute channels.
        channels->insert(std::make_pair(id, channel));
        auto f1 = push(std::move(message));
        auto f2 = f1.then([channel](future_t<void>& f){
            f.get();
            return std::make_tuple(channel->tx, channel->rx);
        });

        return f2;
    }

    auto push(std::uint64_t span, io::encoder_t::message_type&& message) -> future_t<void>;
    auto push(io::encoder_t::message_type&& message) -> future_t<void>;

    std::shared_ptr<basic_channel_t> revoke(std::uint64_t) {
        return nullptr;
    }

private:
    void on_connect(const std::error_code& ec, promise_t<std::error_code>& promise, std::unique_ptr<socket_type>& s);
    void on_read(const std::error_code& ec);
};

template<class Event>
struct traverse {
    typedef std::tuple<
        std::shared_ptr<sender<typename io::event_traits<Event>::dispatch_type>>,
        std::nullptr_t
    > channel_type;
};

template<class T, class BasicSession = basic_session_t>
class session : public std::enable_shared_from_this<session<T, BasicSession>> {
public:
    typedef T tag_type;
    typedef BasicSession basic_session_type;

private:
    std::shared_ptr<basic_session_type> d;
    std::unordered_map<endpoint_t, std::vector<std::shared_ptr<promise_t<void>>>> queue;

public:
    session(std::shared_ptr<basic_session_type> d) :
        d(d)
    {}

    bool connected() const {
        return d->connected();
    }

    auto connect(const endpoint_t& endpoint) -> future_t<void> {
        auto p = std::make_shared<promise_t<void>>();
        auto future = p->get_future();

        auto this_ = this->shared_from_this();
        d->connect(endpoint).then([this_, p, endpoint](future_t<std::error_code>& f){
            auto ec = f.get();

            if (ec) {
                switch (ec.value()) {
                case io_provider::error::already_started:
                    this_->queue[endpoint].push_back(p);
                    break;
                case io_provider::error::already_connected:
                    // TODO: Return ready future.
                    break;
                default:
                    p->set_exception(std::system_error(ec));
                    for (auto& it : this_->queue[endpoint]) {
                        it->set_exception(std::system_error(ec));
                    }
                    this_->queue.erase(endpoint);
                }
            } else {
                p->set_value();
                for (auto& it : this_->queue[endpoint]) {
                    it->set_value();
                }
                this_->queue.erase(endpoint);
            }
        });

        return future;
    }

    template<class Event, class... Args>
    future_t<typename traverse<Event>::channel_type>
    invoke(Args&&...) {
        typedef typename traverse<Event>::channel_type result_type;
        return future_t<result_type>();
    }
};

} // namespace framework

} // namespace cocaine
