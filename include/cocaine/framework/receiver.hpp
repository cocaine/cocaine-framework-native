#pragma once

#include <array>
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

#include "cocaine/framework/error.hpp"
#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/message.hpp"
#include "cocaine/framework/receiver.inl.hpp"

namespace cocaine {

namespace framework {

/// The basic receiver provides an interface to extract incoming MessagePack'ed payloads from the
/// session.
template<class Session>
class basic_receiver_t {
    std::uint64_t id;
    std::shared_ptr<Session> session;
    std::shared_ptr<shared_state_t> state;

public:
    basic_receiver_t(std::uint64_t id, std::shared_ptr<Session> session, std::shared_ptr<shared_state_t> state);

    ~basic_receiver_t();

    /// Returns a future with a decoded message received from the session.
    ///
    /// This future may throw std::system_error on any network failure.
    auto recv() -> task<decoded_message>::future_type;
};

template<class T, class Session>
class receiver {
public:
    typedef T tag_type;
    typedef Session session_type;

private:
    typedef typename detail::result_of<receiver<T, session_type>>::type result_type;
    typedef std::function<result_type(std::shared_ptr<basic_receiver_t<session_type>>, const msgpack::object&)> unpacker_type;

    static const std::array<unpacker_type, boost::mpl::size<typename result_type::types>::value> unpackers;

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

        if (id >= unpackers.size()) {
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
    typedef typename detail::variant_of<tag_type>::type result_type;
    typedef std::function<result_type(const msgpack::object&)> unpacker_type;

    static const std::array<unpacker_type, boost::mpl::size<typename result_type::types>::value> unpackers;

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

        if (id >= unpackers.size()) {
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
const std::array<
    typename receiver<T, Session>::unpacker_type,
    boost::mpl::size<typename receiver<T, Session>::result_type::types>::value
> receiver<T, Session>::unpackers =
    meta::to_array<typename receiver<T, Session>::result_type::types, detail::unpacker_factory<Session, typename receiver<T, Session>::unpacker_type>>::make();

template<class T, class Session>
const std::array<
    typename receiver<io::streaming_tag<T>, Session>::unpacker_type,
    boost::mpl::size<typename receiver<io::streaming_tag<T>, Session>::result_type::types>::value
> receiver<io::streaming_tag<T>, Session>::unpackers =
    meta::to_array<typename receiver<io::streaming_tag<T>, Session>::result_type::types, detail::unpacker_factory<Session, typename receiver<io::streaming_tag<T>, Session>::unpacker_type>>::make();

} // namespace framework

} // namespace cocaine
