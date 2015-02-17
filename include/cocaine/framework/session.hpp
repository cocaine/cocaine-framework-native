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
#include "cocaine/framework/detail/channel.hpp"

// TMP!
#include "cocaine/framework/detail/log.hpp"

namespace cocaine {

namespace framework {

namespace detail {

class executor_type {
    loop_t& loop() {}

    void post() {}
};

}

/*!
 * \note I can't guarantee lifetime safety in other way than by making this class living as shared
 * pointer. The reason is: in particular case the connection's event loop runs in a separate
 * thread, other in that the connection itself lives.
 * Thus no one can guarantee that all asynchronous operations are completed before the connection
 * instance be destroyed.
 *
 * \thread_safety safe.
 */
// TODO: Thing, that sends/receives messages & thing, that manages with channel map.
class basic_session_t : public std::enable_shared_from_this<basic_session_t> {
    typedef asio::ip::tcp protocol_type;
    typedef protocol_type::socket socket_type;
    typedef detail::channel<protocol_type, io::encoder_t, detail::decoder_t> channel_type;

    typedef std::function<void(std::error_code)> callback_type;

    enum class state_t {
        disconnected,
        connecting,
        connected
    };

    loop_t& loop;

    std::unique_ptr<channel_type> channel;

    mutable std::mutex mutex;
    std::atomic<state_t> state;

    std::atomic<std::uint64_t> counter;

    detail::decoder_t::message_type message;
    synchronized<std::unordered_map<std::uint64_t, std::shared_ptr<detail::shared_state_t>>> channels;

//    class impl;
//    std::unique_ptr<impl> d;

    class push_t;
public:
    typedef std::tuple<
        std::shared_ptr<basic_sender_t<basic_session_t>>,
        std::shared_ptr<basic_receiver_t<basic_session_t>>
    > basic_invocation_result;

    /*!
     * \note the event loop reference should be valid until all asynchronous operations complete
     * otherwise the behavior is undefined.
     */
    basic_session_t(loop_t& loop) noexcept;

    ~basic_session_t();

    /*!
     * \note the class does passive connection monitoring, e.g. it won't be immediately notified
     * if the real connection has been lost, but after the next send/recv attempt.
     */
    bool connected() const noexcept;

    // TODO: Make overload `connect(const std::vector<endpoint_t>&)`.
    auto connect(const endpoint_t& endpoint) -> future_t<std::error_code>;

    /*!
     * \brief Emits a disconnection request to the current session.
     *
     * All pending requests should result in operation aborted error.
     */
    void disconnect();

    /*!
     * \note if the future returned throws an exception that means that the data will never be
     * received, but if it doesn't - the data is not guaranteed to be received. It is possible for
     * the other end of connection to hang up immediately after the future returns ok.
     *
     * If you send a **mute** event, there is no way to obtain guarantees of successful message
     * transporting.
     */
    template<class Event, class... Args>
    auto invoke(Args&&... args) -> future_t<basic_invocation_result> {
        const std::uint64_t span(next());
        return invoke(span, io::encoded<Event>(span, std::forward<Args>(args)...));
    }

    auto push(std::uint64_t span, io::encoder_t::message_type&& message) -> future_t<void>;
    auto push(io::encoder_t::message_type&& message) -> future_t<void>;

    void revoke(std::uint64_t span);

private:
    auto next() -> std::uint64_t;
    auto invoke(std::uint64_t span, io::encoder_t::message_type&& message) -> future_t<basic_invocation_result>;

    void on_connect(const std::error_code& ec, promise_t<std::error_code>& promise, std::unique_ptr<socket_type>& s);
    void on_read(const std::error_code& ec);
    void on_error(const std::error_code& ec);
};

template<class T, class BasicSession = basic_session_t>
class session : public std::enable_shared_from_this<session<T, BasicSession>> {
public:
    typedef T tag_type;
    typedef BasicSession basic_session_type;

private:
    std::shared_ptr<basic_session_type> d;
    std::vector<std::shared_ptr<promise_t<void>>> queue;

public:
    // TODO: Non template code - decompose.
    session(std::shared_ptr<basic_session_type> d) :
        d(d)
    {}

    // TODO: Non template code - decompose.
    bool connected() const {
        return d->connected();
    }

    // TODO: Accept vector<endpoint_t>.
    // TODO: Non template code - decompose.
    auto connect(const endpoint_t& endpoint) -> future_t<void> {
        // TODO: Make sure that the connection queue contains tokens for only single endpoint.
        auto p = std::make_shared<promise_t<void>>();
        auto future = p->get_future();

        auto this_ = this->shared_from_this();
        d->connect(endpoint).then([this_, p](future_t<std::error_code>& f){
            auto ec = f.get();
            CF_DBG("basic session connect event: %s", CF_EC(ec));

            if (ec) {
                switch (ec.value()) {
                case io_provider::error::already_started:
                    this_->queue.push_back(p);
                    break;
                case io_provider::error::already_connected:
                    // TODO: Return ready future.
                    COCAINE_ASSERT(false);
                    break;
                default:
                    p->set_exception(std::system_error(ec));
                    for (auto& it : this_->queue) {
                        it->set_exception(std::system_error(ec));
                    }
                    this_->queue.clear();
                }
            } else {
                p->set_value();
                for (auto& it : this_->queue) {
                    it->set_value();
                }
                this_->queue.clear();
            }
        });

        return future;
    }

    // TODO: Non template code - decompose.
    void disconnect() {
        d->disconnect();
    }

    // TODO: Check Event is in T.
    template<class Event, class... Args>
    future_t<
        std::tuple<
            sender<typename io::event_traits<Event>::dispatch_type, basic_session_t>,
            receiver<typename io::event_traits<Event>::upstream_type, basic_session_t>
        >
    >
    invoke(Args&&... args) {
        typedef future_t<
            std::tuple<
                std::shared_ptr<basic_sender_t<basic_session_t>>,
                std::shared_ptr<basic_receiver_t<basic_session_t>>
            >
        > f_type;
        return d->template invoke<Event>(std::forward<Args>(args)...).then([](f_type& f){
            auto ch = f.get();
            auto tx = std::get<0>(ch);
            auto rx = std::get<1>(ch);
            typedef sender<typename io::event_traits<Event>::dispatch_type, basic_session_t> sender_type;
            typedef receiver<typename io::event_traits<Event>::upstream_type, basic_session_t> receiver_type;
            sender_type ttx(tx);
            receiver_type rrx(rx);
            return std::make_tuple(std::move(ttx), std::move(rrx));
        });
    }
};

} // namespace framework

} // namespace cocaine
