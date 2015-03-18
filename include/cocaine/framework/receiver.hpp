#pragma once

#include <functional>
#include <string>
#include <system_error>
#include <queue>

#include <boost/mpl/at.hpp>
#include <boost/mpl/lambda.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/variant.hpp>

#include <cocaine/common.hpp>
#include <cocaine/idl/primitive.hpp>
#include <cocaine/idl/streaming.hpp>
#include <cocaine/rpc/asio/decoder.hpp>

#include "cocaine/framework/error.hpp"
#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/message.hpp"
#include "cocaine/framework/receiver.inl.hpp"

namespace cocaine {

namespace framework {

template<class Session>
class basic_receiver_t {
    std::uint64_t id;
    std::shared_ptr<Session> session;
    std::shared_ptr<shared_state_t> state;

public:
    basic_receiver_t(std::uint64_t id, std::shared_ptr<Session> session, std::shared_ptr<shared_state_t> state);

    ~basic_receiver_t();

    auto recv() -> task<decoded_message>::future_type;
};

namespace detail {

/// Provides information about one of possible type, which can be returned by a receiver with a
/// generic tag, if the received message has Event type.
///
/// In the common returns a tuple containing the next receiver and the current result.
///
/// Do not use this trait with top-level event type.
///
/// \internal
template<class Session, class Event, class = void>
struct receiver_result_item {
private:
    typedef typename io::event_traits<Event>::argument_type argument_type;
    typedef typename io::event_traits<Event>::dispatch_type dispatch_type;

    typedef receiver<dispatch_type, Session> receiver_type;

public:
    typedef std::tuple<receiver_type, typename tuple::fold<argument_type>::type> type;
};

template<class Event>
struct receiver_result_item_streaming_or_primitive {
private:
    typedef typename io::event_traits<Event>::argument_type argument_type;

public:
    typedef typename tuple::fold<argument_type>::type type;
};

/// T... -> list<receiver<E>, R>...
/// \internal
template<class Session, class Messages>
struct receiver_result {
    typedef typename boost::mpl::transform<
        Messages,
        receiver_result_item<Session, boost::mpl::_1>
    >::type type;
};

} // namespace detail

/// Transforms tag type into a variant with all possible pairs of receivers with its result.
/// \internal
template<class Session, class T>
struct receiver_of {
    typedef typename boost::make_variant_over<
        typename detail::receiver_result<Session, typename io::protocol<T>::messages>::type
    >::type type;
};

template<class T>
struct variant_of {
    typedef typename boost::make_variant_over<
        typename boost::mpl::transform<
            typename io::protocol<T>::messages,
            detail::receiver_result_item_streaming_or_primitive<boost::mpl::_1>
        >::type
    >::type type;
};

/// Unpacks message pack object into concrete types.
/// \internal
template<class T, class Session>
class slot_unpacker {
public:
    typedef typename receiver_of<Session, T>::type variant_type;
    typedef std::function<variant_type(std::shared_ptr<basic_receiver_t<Session>>, const msgpack::object&)> function_type;

public:
    slot_unpacker(std::vector<function_type>& unpackers) :
        unpackers(unpackers)
    {}

    template<class U>
    void operator()(U*) {
        unpackers.emplace_back(&slot_unpacker::unpacker<U>::unpack);
    }

private:
    std::vector<function_type>& unpackers;

public:
    template<typename>
    struct unpacker;

    template<typename Tag, typename... Args>
    struct unpacker<std::tuple<receiver<Tag, Session>, std::tuple<Args...>>> {
        static
        std::tuple<receiver<Tag, Session>, std::tuple<Args...>>
        unpack(std::shared_ptr<basic_receiver_t<Session>> d, const msgpack::object& message) {
            std::tuple<Args...> result;
            io::type_traits<std::tuple<Args...>>::unpack(message, result);
            return std::make_tuple(std::move(d), std::move(result));
        }
    };
};

template<class T, class Session>
class slot_unpacker<io::streaming_tag<T>, Session> {
public:
    typedef typename variant_of<io::streaming_tag<T>>::type variant_type;
    typedef std::function<variant_type(const msgpack::object&)> function_type;

public:
    slot_unpacker(std::vector<function_type>& unpackers) :
        unpackers(unpackers)
    {}

    template<class U>
    void operator()(U*) {
        unpackers.emplace_back(&slot_unpacker::unpacker<U>::unpack);
    }

private:
    std::vector<function_type>& unpackers;

public:
    template<class U>
    struct unpacker;

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

template<class T, class Session>
class slot_unpacker<io::primitive_tag<T>, Session> {
public:
    typedef typename variant_of<io::primitive_tag<T>>::type variant_type;
    typedef std::function<variant_type(const msgpack::object&)> function_type;

public:
    slot_unpacker(std::vector<function_type>& unpackers) :
        unpackers(unpackers)
    {}

    template<class U>
    void operator()(U*) {
        unpackers.emplace_back(&slot_unpacker::unpacker<U>::unpack);
    }

private:
    std::vector<function_type>& unpackers;

public:
    template<class U>
    struct unpacker;

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

template<class T, class Session>
static
std::vector<typename slot_unpacker<T, Session>::function_type>
make_slot_unpacker() {
    typedef slot_unpacker<T, Session> unpacker_type;

    std::vector<typename unpacker_type::function_type> result;

    boost::mpl::for_each<
        typename boost::mpl::transform<
            typename unpacker_type::variant_type::types,
            std::add_pointer<boost::mpl::_1>
        >::type
    >(unpacker_type(result));

    return result;
}

template<class T>
struct unpacked_result;

template<class T>
struct unpacked_result<std::tuple<T>> {
    typedef T type;

    static inline
    T unpack(std::tuple<T>& from) {
        return std::get<0>(from);
    }
};

template<class... Args>
struct unpacked_result<std::tuple<Args...>> {
    typedef std::tuple<Args...> type;

    static inline
    std::tuple<Args...> unpack(std::tuple<Args...>& from) {
        return from;
    }
};

/// Helper trait, that simplifies event receiving, that use one of the common protocols.
template<class T>
struct from_receiver_result;

template<class T>
struct from_receiver_result<io::primitive_tag<T>> {
private:
    typedef typename variant_of<io::primitive_tag<T>>::type variant_type;

    typedef typename boost::mpl::at<typename variant_type::types, boost::mpl::int_<0>>::type value_type;
    typedef typename boost::mpl::at<typename variant_type::types, boost::mpl::int_<1>>::type error_type;

public:
    typedef typename unpacked_result<value_type>::type result_type;

    static
    result_type
    transform(variant_type& value) {
        return boost::apply_visitor(visitor_t(), value);
    }

private:
    struct visitor_t : public boost::static_visitor<result_type> {
        result_type operator()(value_type& value) const {
            return unpacked_result<value_type>::unpack(value);
        }

        result_type operator()(error_type& error) const {
            throw cocaine::framework::response_error(error);
        }
    };
};

template<class T>
struct from_receiver_result<io::streaming_tag<T>> {
private:
    typedef typename variant_of<io::streaming_tag<T>>::type variant_type;

    typedef typename boost::mpl::at<typename variant_type::types, boost::mpl::int_<0>>::type value_type;
    typedef typename boost::mpl::at<typename variant_type::types, boost::mpl::int_<1>>::type error_type;
    typedef typename boost::mpl::at<typename variant_type::types, boost::mpl::int_<2>>::type choke_type;

public:
    typedef boost::optional<typename unpacked_result<value_type>::type> result_type;

    static
    result_type
    transform(variant_type& value) {
        return boost::apply_visitor(visitor_t(), value);
    }

private:
    struct visitor_t : public boost::static_visitor<result_type> {
        result_type operator()(value_type& value) const {
            return boost::make_optional(unpacked_result<value_type>::unpack(value));
        }

        result_type operator()(error_type& error) const {
            throw cocaine::framework::response_error(error);
        }

        result_type operator()(choke_type&) const {
            return boost::none;
        }
    };
};

template<class T, class Session>
class receiver {
    typedef Session session_type;

public:
    typedef T tag_type;

    typedef typename receiver_of<session_type, T>::type possible_receivers;

private:
    typedef std::function<possible_receivers(std::shared_ptr<basic_receiver_t<session_type>>, const msgpack::object&)> unpacker_type;

    static const std::vector<unpacker_type> unpackers;

    std::shared_ptr<basic_receiver_t<session_type>> d;

public:
    receiver(std::shared_ptr<basic_receiver_t<session_type>> d) :
        d(std::move(d))
    {}

    auto recv() -> typename task<possible_receivers>::future_type {
        BOOST_ASSERT(this->d);

        auto d = std::move(this->d);
        auto future = d->recv();
        return future
            .then(std::bind(&receiver::convert, std::placeholders::_1, std::move(d)));
    }

private:
    static inline
    possible_receivers
    convert(task<decoded_message>::future_move_type future, std::shared_ptr<basic_receiver_t<session_type>> d) {
        const auto message = future.get();
        const auto id = message.type();

        if (id >= static_cast<std::size_t>(boost::mpl::size<typename possible_receivers::types>::value)) {
            throw std::runtime_error("invalid protocol");
        }

        return unpackers[id](std::move(d), message.args());
    }
};

/// \helper
template<class T, class Session>
class receiver<io::primitive_tag<T>, Session> {
    typedef io::primitive_tag<T> tag_type;
    typedef Session session_type;

    typedef typename variant_of<tag_type>::type possible_receivers;

    typedef std::function<possible_receivers(const msgpack::object&)> unpacker_type;
    static const std::vector<unpacker_type> unpackers;
    std::shared_ptr<basic_receiver_t<session_type>> d;

public:
    receiver(std::shared_ptr<basic_receiver_t<session_type>> d) :
        d(std::move(d))
    {}

    auto recv() -> typename task<typename from_receiver_result<tag_type>::result_type>::future_type {
        COCAINE_ASSERT(this->d);

        auto d = std::move(this->d);
        auto future = d->recv();
        return future
            .then(std::bind(&receiver::convert, std::placeholders::_1, std::move(d)));
    }

private:
    static inline
    typename from_receiver_result<tag_type>::result_type
    convert(task<decoded_message>::future_move_type future, std::shared_ptr<basic_receiver_t<session_type>>) {
        const auto message = future.get();
        const auto id = message.type();

        if (id >= static_cast<std::size_t>(boost::mpl::size<typename possible_receivers::types>::value)) {
            throw std::runtime_error("invalid protocol");
        }

        auto payload = unpackers[id](message.args());
        return from_receiver_result<tag_type>::transform(payload);
    }
};

/// \helper
template<class T, class Session>
class receiver<io::streaming_tag<T>, Session> {
    typedef Session session_type;

    typedef typename variant_of<io::streaming_tag<T>>::type possible_receivers;

    typedef std::function<possible_receivers(const msgpack::object&)> unpacker_type;
    static const std::vector<unpacker_type> unpackers;
    std::shared_ptr<basic_receiver_t<session_type>> d;

public:
    receiver(std::shared_ptr<basic_receiver_t<session_type>> d) :
        d(std::move(d))
    {}

    // TODO: Not copyable, but doesn't invalidates itself on recv!

    auto recv() -> typename task<boost::optional<std::string>>::future_type {
        auto future = d->recv();
        return future
            .then(std::bind(&receiver::convert, std::placeholders::_1, d));
    }

private:
    static inline
    boost::optional<std::string>
    convert(task<decoded_message>::future_move_type future, std::shared_ptr<basic_receiver_t<session_type>>) {
        const auto message = future.get();
        const auto id = message.type();

        if (id >= static_cast<std::size_t>(boost::mpl::size<typename possible_receivers::types>::value)) {
            throw std::runtime_error("invalid protocol");
        }

        auto payload = unpackers[id](message.args());
        return from_receiver_result<io::streaming_tag<T>>::transform(payload);
    }
};

template<class Session>
class receiver<void, Session> {
public:
    receiver(std::shared_ptr<basic_receiver_t<Session>>) {}
};

template<class T, class Session>
const std::vector<typename receiver<T, Session>::unpacker_type>
receiver<T, Session>::unpackers = make_slot_unpacker<T, Session>();

template<class T, class Session>
const std::vector<typename receiver<io::primitive_tag<T>, Session>::unpacker_type>
receiver<io::primitive_tag<T>, Session>::unpackers = make_slot_unpacker<io::primitive_tag<T>, Session>();

template<class T, class Session>
const std::vector<typename receiver<io::streaming_tag<T>, Session>::unpacker_type>
receiver<io::streaming_tag<T>, Session>::unpackers = make_slot_unpacker<io::streaming_tag<T>, Session>();

} // namespace framework

} // namespace cocaine
