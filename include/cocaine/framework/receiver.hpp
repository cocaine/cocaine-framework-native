#pragma once

#include <string>
#include <system_error>
#include <queue>

#include <boost/mpl/lambda.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/variant.hpp>

#include <cocaine/common.hpp>
#include <cocaine/idl/primitive.hpp>
#include <cocaine/idl/streaming.hpp>
#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/tuple.hpp>

#include <cocaine/framework/common.hpp>
#include <cocaine/framework/forwards.hpp>

#include "cocaine/framework/detail/log.hpp"

namespace cocaine {

namespace framework {

// Forwards.
class basic_session_t;

namespace detail {

/*!
 * Transforms a typelist sequence into a single movable argument type.
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
///              -> variant<T, tuple<int, string>, tuple<>>
template<class T>
struct result_of;

template<class T>
struct result_of<io::primitive_tag<T>> {
    typedef typename packable<T>::type value_type;
    typedef typename packable<typename io::primitive<T>::error::argument_type>::type error_type;

    typedef boost::variant<value_type, error_type> type;
};

template<class T>
struct result_of<io::streaming_tag<T>> {
    typedef typename packable<T>::type chunk_type;
    typedef typename packable<typename io::streaming<T>::error::argument_type>::type error_type;
    typedef std::tuple<> choke_type;

    typedef boost::variant<chunk_type, error_type, choke_type> type;
};

} // namespace detail

template<class T>
struct result_of {
    typedef typename detail::result_of<T>::type type;
};

template<class Event> class channel_t;

template<class T>
class slot_unpacker {
    typedef typename result_of<T>::type result_type;
    typedef std::function<result_type(const msgpack::object&)> function_type;

public:
    static std::vector<function_type> generate() {
        std::vector<function_type> result;
        boost::mpl::for_each<typename result_type::types>(slot_unpacker<T>(result));
        return result;
    }

    template<class U>
    void operator()(const U&) {
        unpackers.emplace_back(&slot_unpacker::unpacker<U>::unpack);
    }

private:
    std::vector<function_type>& unpackers;

    slot_unpacker(std::vector<function_type>& unpackers) :
        unpackers(unpackers)
    {}

public:
    template<typename R>
    struct unpacker {
        static
        R
        unpack(const msgpack::object& message) {
            std::tuple<R> result;
            io::type_traits<std::tuple<R>>::unpack(message, result);
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

class basic_session_t;

class basic_receiver_t {
    friend class basic_session_t;
    typedef io::decoder_t::message_type result_type;

    std::uint64_t id;
    std::shared_ptr<basic_session_t> session;

    boost::optional<std::error_code> broken;

    std::mutex mutex;
    std::queue<result_type> queue;
    std::queue<promise_t<result_type>> pending;

public:
    basic_receiver_t(std::uint64_t id, std::shared_ptr<basic_session_t> session);

    ~basic_receiver_t();

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
        const auto payload = message;

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
            pending.front().set_exception(std::system_error(ec));
            pending.pop();
        }
    }
};

template<class T>
class receiver {
    typedef typename result_of<T>::type result_type;
    typedef typename result_type::types result_typelist;

    typedef std::function<result_type(const msgpack::object&)> unpacker_type;

    static const std::vector<unpacker_type> visitors;

    std::shared_ptr<basic_receiver_t> d;

public:
    receiver(std::shared_ptr<basic_receiver_t> d) :
        d(d)
    {}

    // Intentionally deleted.
    receiver(const receiver& other) = default;
    receiver(receiver&& other) = default;

    receiver& operator=(const receiver& other) = default;
    receiver& operator=(receiver&& other) = default;

    // recv<scope::value,error>
    // recv<scope::chunk|choke|error>
    // TODO: Return improved variant with typechecking.
    future_t<result_type>
    recv() {
        auto message = d->recv().get();
        CF_DBG("recv, message: %llu", message.type());
        // TODO: Check type.
        // TODO: Make it work with single types.
        // TODO: More convenient interface.

        const auto id = message.type();
        if (id >= boost::mpl::size<result_typelist>::value) {
            // TODO: What to do? Notify the user, I think.
            // std::cout << "dropping a " << id << " type message" << std::endl;
            // return;
        }

        const auto payload = visitors[id](message.args());
        return make_ready_future<result_type>::value(payload);
    }
};

template<class T>
const std::vector<typename receiver<T>::unpacker_type> receiver<T>::visitors = slot_unpacker<T>::generate();

} // namespace framework

} // namespace cocaine
