#pragma once

#include <cstdint>
#include <iostream>
#include <queue>
#include <unordered_map>

#include <boost/mpl/lambda.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/variant.hpp>

#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>

#include <cocaine/common.hpp>
#include <cocaine/locked_ptr.hpp>
#include <cocaine/rpc/asio/channel.hpp>
#include <cocaine/idl/primitive.hpp>
#include <cocaine/rpc/result_of.hpp>
#include <cocaine/tuple.hpp>

#include <cocaine/traits/dynamic.hpp>
#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/graph.hpp>
#include <cocaine/traits/tuple.hpp>
#include <cocaine/traits/map.hpp>
#include <cocaine/traits/vector.hpp>

#include "cocaine/framework/util/future.hpp"

namespace cocaine {

namespace framework {

template<typename T> struct deduced_type;

namespace detail {

/*!
 * Transforms a typelist sequence to a single movable argument type.
 *
 * If the sequence contains a single element of type T, than the result type will be T.
 * Otherwise the sequence will be transformed into a tuple.
 *
 * sequence<T, U...> -> std::tuple<T, U...>,
 * sequence<T>       -> T.
 */
template<class U, size_t = boost::mpl::size<U>::value>
struct packable {
    typedef typename tuple::fold<U>::type type;
};

template<class U>
struct packable<U, 1> {
    typedef typename boost::mpl::front<U>::type type;
};

/// primitive<T> -> variant<value<T>::type, error::type>
///              -> variant<T, tuple<int, string>>
/// streaming<T> -> variant<chunk<T>::type, error::type, choke::type>
///              -> variant<T, tuple<int, string>, void>
template<class T>
struct result_of;

template<class T>
struct result_of<io::primitive_tag<T>> {
    typedef typename packable<T>::type value_type;
    typedef typename packable<typename io::primitive<T>::error::argument_type>::type error_type;

    typedef boost::variant<value_type, error_type> type;
};

// TODO: Not implemented yet.
template<class T>
struct result_of<io::streaming_tag<T>>;

} // namespace detail

template<class Event>
struct result_of {
    typedef typename detail::result_of<
        typename io::event_traits<Event>::upstream_type
    >::type type;
};

using loop_t = asio::io_service;
using endpoint_t = asio::ip::tcp::endpoint;

template<typename T>
using promise_t = promise<T>;

template<typename T>
using future_t = future<T>;

template<class Event> class channel_t;

template<class Event>
class slot_unpacker {
    typedef typename result_of<Event>::type result_type;
    typedef std::function<result_type(const msgpack::object&)> function_type;

public:
    static std::vector<function_type> generate() {
        std::vector<function_type> result;
        boost::mpl::for_each<typename result_type::types>(slot_unpacker<Event>(result));
        return result;
    }

    template<class T>
    void operator()(const T&) {
        unpackers.emplace_back(&slot_unpacker::unpacker<T>::unpack);
    }

private:
    std::vector<function_type>& unpackers;

    slot_unpacker(std::vector<function_type>& unpackers) :
        unpackers(unpackers)
    {}

private:
    template<typename T>
    struct unpacker {
        static
        T
        unpack(const msgpack::object& message) {
            std::tuple<T> result;
            io::type_traits<std::tuple<T>>::unpack(message, result);
            return std::get<0>(result);
        }
    };

    template<typename... Args>
    struct unpacker<std::tuple<Args...>> {
        static
        std::tuple<Args...>
        unpack(const msgpack::object& message) {
            std::tuple<Args...> result;
            io::type_traits<std::tuple<Args...>>::unpack(message, result);
            return result;
        }
    };
};

template<class Event>
class receiver {
    typedef Event event_type;
    friend class channel_t<event_type>;

    typedef typename result_of<event_type>::type result_type;
    typedef typename result_type::types result_typelist;

    typedef std::function<result_type(const msgpack::object&)> unpacker_type;

    boost::optional<std::error_code> broken;

    std::mutex mutex;
    std::queue<result_type> queue;
    std::queue<promise_t<result_type>> pending;

    static const std::vector<unpacker_type> visitors;

public:
    // TODO: Return improved variant with typechecking.
    future_t<result_type> recv() {
        promise_t<result_type> promise;
        auto future = promise.get_future();

        std::lock_guard<std::mutex> lock(mutex);
        if (broken) {
            COCAINE_ASSERT(queue.empty());

            promise.set_exception(std::system_error(broken.get()));
            return future;
        }

        if (queue.empty()) {
            pending.push(std::move(promise));
        } else {
            promise.set_value(std::move(queue.front()));
            queue.pop();
        }

        return future;
    }

private:
    void push(io::decoder_t::message_type&& message) {
        const auto id = message.type();
        if (id >= boost::mpl::size<result_typelist>::value) {
            // TODO: What to do? Notify the user, I think.
            return;
        }

        const auto payload = visitors[id](message.args());

        std::lock_guard<std::mutex> lock(mutex);
        COCAINE_ASSERT(!broken);

        if (pending.empty()) {
            queue.push(payload);
        } else {
            auto promise = std::move(pending.front());
            promise.set_value(payload);
            pending.pop();
        }
    }

    void error(const std::error_code& ec) {
        std::lock_guard<std::mutex> lock(mutex);
        broken = ec;
        while (!pending.empty()) {
            auto promise = std::move(pending.front());
            promise.set_exception(std::system_error(ec));
            pending.pop();
        }
    }
};

template<class Event>
const std::vector<typename receiver<Event>::unpacker_type> receiver<Event>::visitors = slot_unpacker<Event>::generate();

class basic_channel_t {
    std::uint64_t id;

public:
    basic_channel_t(std::uint64_t id) : id(id) {}
    virtual ~basic_channel_t() {}

    virtual void process(io::decoder_t::message_type&& message) = 0;
    virtual void error(const std::error_code& ec) = 0;
};

template<class Event>
class channel_t : public basic_channel_t {
public:
    std::shared_ptr<receiver<Event>> rx;

    channel_t(std::uint64_t id) :
        basic_channel_t(id),
        rx(new receiver<Event>())
    {}

    void process(io::decoder_t::message_type&& message) {
        rx->push(std::move(message));
    }

    void error(const std::error_code& ec) {
        rx->error(ec);
    }
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

    enum class state_t {
        disconnected,
        connecting,
        connected
    };

    loop_t& loop;

    std::atomic<state_t> state;
    std::unique_ptr<socket_type> socket;

    mutable std::mutex connection_queue_mutex;
    std::vector<promise_t<void>> connection_queue;

    std::atomic<std::uint64_t> counter;
    std::shared_ptr<channel_type> channel;

    mutable std::mutex channel_mutex;

    io::decoder_t::message_type message;
    synchronized<std::unordered_map<std::uint64_t, std::shared_ptr<basic_channel_t>>> channels;

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
    std::shared_ptr<receiver<T>> invoke(Args&&... args) {
        const auto id = counter++;
        auto message = io::encoded<T>(id, std::forward<Args>(args)...);
        auto channel = std::make_shared<channel_t<T>>(id);

        // TODO: Do not insert mute channels.
        channels->insert(std::make_pair(id, channel));
        invoke(std::move(message));
        return channel->rx;
    }

private:
    void invoke(io::encoder_t::message_type&& message);

    void on_connected(const std::error_code& ec);
    void on_read(const std::error_code& ec);
};

} // namespace framework

} // namespace cocaine
