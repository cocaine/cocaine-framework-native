#pragma once

#include <string>
#include <system_error>
#include <functional>
#include <queue>

#include <boost/mpl/at.hpp>
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
#include <cocaine/detail/service/node/messages.hpp>

#include "cocaine/framework/error.hpp"
#include "cocaine/framework/forwards.hpp"

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/detail/decoder.hpp"

namespace cocaine {

namespace framework {

// Forwards.
class basic_session_t;

namespace detail {

class shared_state_t {
public:
    typedef decoder_t::message_type value_type;

    std::queue<value_type> queue;
    std::queue<typename task<value_type>::promise_type> pending;

    boost::optional<std::error_code> broken;

    std::mutex mutex;

public:
    void put(value_type&& message) {
        std::lock_guard<std::mutex> lock(mutex);

        COCAINE_ASSERT(!broken);

        if (pending.empty()) {
            queue.push(std::move(message));
        } else {
            pending.front().set_value(std::move(message));
            pending.pop();
        }
    }

    void put(const std::error_code& ec) {
        std::lock_guard<std::mutex> lock(mutex);

        COCAINE_ASSERT(!broken);

        broken = ec;
        while (!pending.empty()) {
            pending.front().set_exception(std::system_error(ec));
            pending.pop();
        }
    }

    auto get() -> typename task<value_type>::future_type {
        std::lock_guard<std::mutex> lock(mutex);

        if (broken) {
            COCAINE_ASSERT(queue.empty());

            return make_ready_future<value_type>::error(std::system_error(broken.get()));
        }

        if (queue.empty()) {
            typename task<value_type>::promise_type promise;
            auto future = promise.get_future();
            pending.push(std::move(promise));
            return future;
        }

        auto future = make_ready_future<value_type>::value(std::move(queue.front()));
        queue.pop();
        return future;
    }
};

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

/*!
 * primitive<T> -> variant<value<T>::type, error::type>
 *              -> variant<T, tuple<int, string>>
 * streaming<T> -> variant<chunk<T>::type, error::type, choke::type>
 *              -> variant<T, tuple<int, string>, tuple<>>
 */
template<class T>
struct variant_of;

template<class T>
struct variant_of<io::primitive_tag<T>> {
    typedef typename packable<T>::type value_type;
    typedef typename packable<typename io::primitive<T>::error::argument_type>::type error_type;

    typedef boost::variant<value_type, error_type> type;
};

template<class T>
struct variant_of<io::streaming_tag<T>> {
    typedef typename packable<T>::type chunk_type;
    typedef typename packable<typename io::streaming<T>::error::argument_type>::type error_type;
    typedef std::tuple<> choke_type;

    typedef boost::variant<chunk_type, error_type, choke_type> type;
};

} // namespace detail

template<class T>
struct variant_of {
    typedef typename detail::variant_of<T>::type type;
};

template<class T>
class slot_unpacker {
    typedef typename variant_of<T>::type variant_type;
    typedef std::function<variant_type(const msgpack::object&)> function_type;

public:
    static std::vector<function_type> generate() {
        std::vector<function_type> result;
        boost::mpl::for_each<typename variant_type::types>(slot_unpacker<T>(result));
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

template<class Session>
class basic_receiver_t {
//    friend class Session;
    typedef detail::decoder_t::message_type result_type;

    std::uint64_t id;
    std::shared_ptr<Session> session;
    std::shared_ptr<detail::shared_state_t> state;

public:
    basic_receiver_t(std::uint64_t id, std::shared_ptr<Session> session, std::shared_ptr<detail::shared_state_t> state);

    ~basic_receiver_t();

    auto recv() -> typename task<result_type>::future_type;

    void revoke();
};

/*!
 * Helper trait, that simplifies event receiving, that use one of the common protocols.
 *
 * \todo the common trait is undefined yet.
 */
template<class T>
struct receiver_traits;

template<class T>
struct receiver_traits<io::primitive_tag<T>> {
private:
    typedef typename variant_of<io::primitive_tag<T>>::type variant_type;

    typedef typename boost::mpl::at<typename variant_type::types, boost::mpl::int_<0>>::type value_type;
    typedef typename boost::mpl::at<typename variant_type::types, boost::mpl::int_<1>>::type error_type;

public:
    typedef value_type result_type;

    static
    result_type
    convert(variant_type& value) {
        return boost::apply_visitor(visitor_t(), value);
    }

private:
    struct visitor_t : public boost::static_visitor<result_type> {
        result_type operator()(value_type& value) const {
            return value;
        }

        result_type operator()(error_type& error) const {
            throw cocaine_error(error);
        }
    };
};

template<class T>
struct receiver_traits<io::streaming_tag<T>> {
private:
    typedef typename variant_of<io::streaming_tag<T>>::type variant_type;

    typedef typename boost::mpl::at<typename variant_type::types, boost::mpl::int_<0>>::type value_type;
    typedef typename boost::mpl::at<typename variant_type::types, boost::mpl::int_<1>>::type error_type;
    typedef typename boost::mpl::at<typename variant_type::types, boost::mpl::int_<2>>::type choke_type;

public:
    typedef boost::optional<value_type> result_type;

    static
    result_type
    convert(variant_type& value) {
        return boost::apply_visitor(visitor_t(), value);
    }

private:
    struct visitor_t : public boost::static_visitor<result_type> {
        result_type operator()(value_type& value) const {
            return boost::make_optional(value);
        }

        result_type operator()(error_type& error) const {
            throw cocaine_error(error);
        }

        result_type operator()(choke_type&) const {
            return boost::none;
        }
    };
};

template<class T>
class terminator {
    typedef std::function<bool()> function_type;

public:
    static std::vector<function_type> generate() {
        std::vector<function_type> result;
        boost::mpl::for_each<typename io::protocol<T>::messages>(terminator<T>(result));
        return result;
    }

    template<class U>
    void operator()(const U&) {
        unpackers.emplace_back(&terminator::unpacker<U>::unpack);
    }

private:
    std::vector<function_type>& unpackers;

    terminator(std::vector<function_type>& unpackers) :
        unpackers(unpackers)
    {}

public:
    template<typename U>
    struct unpacker {
        static
        bool
        unpack() {
            return std::is_same<typename io::event_traits<U>::dispatch_type, void>::value;
        }
    };
};

template<class T, class Session>
class receiver {
    typedef typename variant_of<T>::type variant_type;
    typedef typename variant_type::types variant_typelist;

    typedef std::function<variant_type(const msgpack::object&)> unpacker_type;
    typedef std::function<bool()> terminator_type;

    static const std::vector<unpacker_type> visitors;
    static const std::vector<terminator_type> terminators;

    std::shared_ptr<basic_receiver_t<Session>> d;

public:
    receiver(std::shared_ptr<basic_receiver_t<Session>> d) :
        d(d)
    {}

    // Intentionally deleted.
    receiver(const receiver& other) = default;
    receiver(receiver&& other) = default;

    receiver& operator=(const receiver& other) = default;
    receiver& operator=(receiver&& other) = default;

    /*!
     * \note does auto revoke when reached a leaf.
     *
     * \warning this receiver will be invalidated after this call.
     */
    typename task<typename receiver_traits<T>::result_type>::future_type
    recv() {
        return d->recv().then(std::bind(&receiver<T, Session>::convert, std::placeholders::_1, d));
    }

private:
    static
    typename receiver_traits<T>::result_type
    convert(typename task<detail::decoder_t::message_type>::future_type& f, std::shared_ptr<basic_receiver_t<Session>> d) {
        const detail::decoder_t::message_type message = f.get();
        const std::uint64_t id = message.type();
        if (id >= boost::mpl::size<variant_typelist>::value) {
            // TODO: What to do? Notify the user, I think.
            CF_DBG("dropping a %llu type message", id);
        }

        variant_type payload = visitors[id](message.args());
        // TODO: Close the channel if it the next node is terminate leaf.
        if (terminators[id]()) {
            CF_DBG("received the last message in the protocol graph - terminating ...");
            d->revoke();
        }
        return receiver_traits<T>::convert(payload);
    }
};

template<class T, class Session>
const std::vector<typename receiver<T, Session>::unpacker_type>
receiver<T, Session>::visitors = slot_unpacker<T>::generate();

template<class T, class Session>
const std::vector<typename receiver<T, Session>::terminator_type>
receiver<T, Session>::terminators = terminator<T>::generate();

} // namespace framework

} // namespace cocaine
