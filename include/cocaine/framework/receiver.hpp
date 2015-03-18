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

template<typename, class>
struct unpacker;

template<typename Tag, class Session, typename... Args>
struct unpacker<std::tuple<receiver<Tag, Session>, std::tuple<Args...>>, Session> {
    std::tuple<receiver<Tag, Session>, std::tuple<Args...>>
    operator()(std::shared_ptr<basic_receiver_t<Session>> d, const msgpack::object& message) {
        return std::make_tuple(std::move(d), unpacker<std::tuple<Args...>, Session>::unpack(message));
    }
};

template<class Session, typename... Args>
struct unpacker<std::tuple<receiver<void, Session>, std::tuple<Args...>>, Session> {
    std::tuple<Args...>
    operator()(std::shared_ptr<basic_receiver_t<Session>>, const msgpack::object& message) {
        return unpacker<std::tuple<Args...>, Session>::unpack(message);
    }
};

template<class Session, typename... Args>
struct unpacker<std::tuple<Args...>, Session> {
    std::tuple<Args...>
    operator()(const msgpack::object& message) {
        std::tuple<Args...> result;
        io::type_traits<std::tuple<Args...>>::unpack(message, result);
        return result;
    }

    std::tuple<Args...>
    operator()(std::shared_ptr<basic_receiver_t<Session>>, const msgpack::object& message) {
        std::tuple<Args...> result;
        io::type_traits<std::tuple<Args...>>::unpack(message, result);
        return result;
    }
};

/// Unpacks message pack object into concrete types.
/// \internal
template<class Receiver>
class slot_unpacker {
public:
    typedef typename detail::result_of<Receiver>::type variant_type;
    typedef std::function<variant_type(std::shared_ptr<basic_receiver_t<typename Receiver::session_type>>, const msgpack::object&)> function_type;

public:
    slot_unpacker(std::vector<function_type>& unpackers) :
        unpackers(unpackers)
    {}

    template<class U>
    void operator()(U*) {
        unpackers.emplace_back(unpacker<U, typename Receiver::session_type>());
    }

private:
    std::vector<function_type>& unpackers;
};

template<class T, class Session>
class slot_unpacker_raw {
public:
    typedef typename detail::variant_of<T>::type variant_type;
    typedef std::function<variant_type(const msgpack::object&)> function_type;

public:
    slot_unpacker_raw(std::vector<function_type>& unpackers) :
        unpackers(unpackers)
    {}

    template<class U>
    void operator()(U*) {
        unpackers.emplace_back(unpacker<U, Session>());
    }

private:
    std::vector<function_type>& unpackers;
};

template<class T, class Session>
class slot_unpacker<receiver<io::streaming_tag<T>, Session>> {
public:
    typedef typename detail::variant_of<io::streaming_tag<T>>::type variant_type;

    typedef std::function<variant_type(const msgpack::object&)> function_type;

public:
    slot_unpacker(std::vector<function_type>& unpackers) :
        unpackers(unpackers)
    {}

    template<class U>
    void operator()(U*) {
        unpackers.emplace_back(unpacker<U, Session>());
    }

private:
    std::vector<function_type>& unpackers;
};

template<class Receiver>
static
std::vector<typename slot_unpacker<Receiver>::function_type>
make_slot_unpacker() {
    typedef slot_unpacker<Receiver> unpacker_type;

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
template<class T, class Session>
struct from_receiver {
private:
    typedef typename detail::result_of<receiver<T, Session>>::type variant_type;

public:
    typedef variant_type result_type;

    static inline
    result_type
    transform(variant_type& value) {
        return value;
    }
};

template<class T, class Session>
struct from_receiver<io::primitive_tag<T>, Session> {
private:
    typedef typename detail::result_of<receiver<io::primitive_tag<T>, Session>>::type variant_type;

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

template<class T, class Session>
struct from_receiver<io::streaming_tag<T>, Session> {
private:
    typedef typename detail::variant_of<io::streaming_tag<T>>::type variant_type;

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
public:
    typedef T tag_type;
    typedef Session session_type;

private:
    typedef typename detail::result_of<receiver<T, session_type>>::type possible_receivers;
    typedef std::function<possible_receivers(std::shared_ptr<basic_receiver_t<session_type>>, const msgpack::object&)> unpacker_type;

    static const std::vector<unpacker_type> unpackers;

    std::shared_ptr<basic_receiver_t<session_type>> d;

public:
    receiver(std::shared_ptr<basic_receiver_t<session_type>> d) :
        d(std::move(d))
    {}

    auto recv() -> typename task<typename from_receiver<T, Session>::result_type>::future_type {
        BOOST_ASSERT(this->d);

        auto d = std::move(this->d);
        auto future = d->recv();
        return future
            .then(std::bind(&receiver::convert, std::placeholders::_1, std::move(d)));
    }

private:
    static inline
    typename from_receiver<T, Session>::result_type
    convert(task<decoded_message>::future_move_type future, std::shared_ptr<basic_receiver_t<session_type>> d) {
        const auto message = future.get();
        const auto id = message.type();

        if (id >= static_cast<std::size_t>(boost::mpl::size<typename possible_receivers::types>::value)) {
            throw std::runtime_error("invalid protocol");
        }

        auto result = unpackers[id](std::move(d), message.args());
        return from_receiver<T, Session>::transform(result);
    }
};

/// \helper
template<class T, class Session>
class receiver<io::streaming_tag<T>, Session> {
public:
    typedef io::streaming_tag<T> tag_type;
    typedef Session session_type;

private:
    typedef typename detail::variant_of<tag_type>::type possible_receivers;

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
        return from_receiver<io::streaming_tag<T>, Session>::transform(payload);
    }
};

template<class Session>
class receiver<void, Session> {
public:
    typedef void tag_type;
    typedef Session session_type;

    receiver(std::shared_ptr<basic_receiver_t<Session>>) {}
};

template<class T, class Session>
const std::vector<typename receiver<T, Session>::unpacker_type>
receiver<T, Session>::unpackers = make_slot_unpacker<receiver<T, Session>>();

template<class T, class Session>
const std::vector<typename receiver<io::streaming_tag<T>, Session>::unpacker_type>
receiver<io::streaming_tag<T>, Session>::unpackers = make_slot_unpacker<receiver<io::streaming_tag<T>, Session>>();

} // namespace framework

} // namespace cocaine
