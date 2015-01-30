#pragma once

#include <boost/mpl/lambda.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/variant.hpp>

#include <cocaine/idl/primitive.hpp>
#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/tuple.hpp>

namespace cocaine {

namespace framework {

// Forwards.
class basic_session_t;

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

template<class T>
struct result_of<io::streaming_tag<T>> {
// TODO: Not implemented yet.
    typedef boost::variant<bool> type;
};

} // namespace detail

template<class Event>
struct result_of {
    typedef typename detail::result_of<
        typename io::event_traits<Event>::upstream_type
    >::type type;
};

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
class basic_receiver_t {
    typedef Event event_type;
    friend class channel_t<event_type>;

    typedef typename result_of<event_type>::type result_type;
    typedef typename result_type::types result_typelist;

    typedef std::function<result_type(const msgpack::object&)> unpacker_type;

    static const std::vector<unpacker_type> visitors;

    boost::optional<std::error_code> broken;

    std::mutex mutex;
    std::queue<result_type> queue;
    std::queue<promise_t<result_type>> pending;

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
            // std::cout << "dropping a " << id << " type message" << std::endl;
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
            pending.front().set_exception(std::system_error(ec));
            pending.pop();
        }
    }
};

template<class Event>
const std::vector<typename basic_receiver_t<Event>::unpacker_type> basic_receiver_t<Event>::visitors = slot_unpacker<Event>::generate();

}

}
