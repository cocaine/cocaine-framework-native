/*
    Copyright (c) 2015 Evgeny Safronov <division494@gmail.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.
    This file is part of Cocaine.
    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

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
public:
    typedef Session session_type;

private:
    std::uint64_t id;
    std::shared_ptr<session_type> session;
    std::shared_ptr<shared_state_t> state;

public:
    basic_receiver_t(std::uint64_t id, std::shared_ptr<session_type> session, std::shared_ptr<shared_state_t> state);

    ~basic_receiver_t();

    /// Returns a future with a decoded message received from the session.
    ///
    /// This future may throw std::system_error on any network failure.
    auto recv() -> task<decoded_message>::future_type;
};

/// The receiver class provides a convenient way to extract typed messages from the session.
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

    /// The copy constructor is deleted intentionally.
    ///
    /// This is necessary, because each recv() method call in the common case invalidates current
    /// object's state and returns the new receiver instead.
    receiver(const receiver& other) = delete;
    receiver(receiver&&) = default;

    /// The copy assignlemt operator is deleted intentionally.
    ///
    /// This is necessary, because each recv() method call in the common case invalidates current
    /// object's state and returns the new receiver instead.
    receiver& operator=(const receiver& other) = delete;
    receiver& operator=(receiver&&) = default;

    /// Performs receive asynchronous operation, extracting the next incoming message from the
    /// receiver channel.
    ///
    /// \returns a future, which contains either the next protocol message or the exception on any
    /// system error.
    ///
    /// The actual result type depends on from_receiver trait. In the common case it returns a
    /// variant containing tuples of pairs of the next receiver object and the result of this
    /// operation.
    /// For example:
    ///     recv() -> future<variant<tuple<receiver<T1>, R1>, tuple<receiver<T2> ,R2>, ...>>.
    ///
    /// If the message received is a terminal message, then there will be no receiver in the tuple.
    /// For example for primitive tags there are from_receiver trait specialization, which gives us
    /// the following behavior:
    ///     recv() -> future<value> | throw.
    ///
    /// For further understanding see \sa detail::result_of.
    ///
    /// \note this method automatically revokes the channel when reached a leaf in the dispatch
    /// graph.
    ///
    /// \warning the current receiver will be invalidated after this call.
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

/// The receiver specialization for streaming tags.
///
/// The main difference is that the receiver doesn't invalidate itself on recv, allowing you to
/// extract chunks using the same receiver.
///
/// Unlike the common receiver here you are allowed to copy it.
///
/// For example:
/// \code{.cpp}
/// while (boost::optional<T> value = rx.recv()) {
///     // Do something with the chunk.
/// }
/// \endcode
///
/// \helper
template<class T, class Session>
class receiver<io::streaming_tag<T>, Session> {
public:
    typedef io::streaming_tag<T> tag_type;
    typedef Session session_type;

private:
    typedef typename detail::variant_of<tag_type>::type result_type;
    typedef std::function<result_type(const msgpack::object&)> unpacker_type;
    typedef std::array<unpacker_type, boost::mpl::size<typename result_type::types>::value> unpackers_type;

    static const unpackers_type unpackers;

    std::shared_ptr<basic_receiver_t<session_type>> d;

public:
    receiver(std::shared_ptr<basic_receiver_t<session_type>> d) :
        d(std::move(d))
    {}

    receiver(const receiver&) = default;
    receiver(receiver&&) = default;

    receiver& operator=(const receiver&) = default;
    receiver& operator=(receiver&&) = default;

    /// Performs receive asynchronous operation, extracting the next incoming message from the
    /// receiver channel.
    ///
    /// \returns a future, which contains the optional value of streaming type or the exception on
    /// any system error.
    ///
    /// For example:
    ///     recv() -> future<optional<T>> | throw.
    auto recv() -> typename task<typename from_receiver<tag_type, Session>::result_type>::future_type {
        auto future = d->recv();
        return future
            .then(std::bind(&receiver::convert, std::placeholders::_1, d));
    }

private:
    static inline
    typename from_receiver<tag_type, Session>::result_type
    convert(task<decoded_message>::future_move_type future, std::shared_ptr<basic_receiver_t<session_type>>) {
        const auto message = future.get();
        const auto id = message.type();

        if (id >= unpackers.size()) {
            throw std::runtime_error("invalid protocol");
        }

        auto payload = unpackers[id](message.args());
        return from_receiver<tag_type, Session>::transform(payload);
    }
};

/// The terminal receiver specialization.
///
/// Does nothing except the dropping internal reference-counted object, which leads to the channel
/// revoking.
template<class Session>
class receiver<void, Session> {
public:
    typedef void tag_type;
    typedef Session session_type;

    receiver(std::shared_ptr<basic_receiver_t<Session>>) {}
};

// Static unpackers variables initialization.
template<class T, class Session>
const std::array<
    typename receiver<T, Session>::unpacker_type,
    boost::mpl::size<typename receiver<T, Session>::result_type::types>::value
> receiver<T, Session>::unpackers =
    meta::to_array<
        typename receiver<T, Session>::result_type::types,
        detail::unpacker_factory<Session, typename receiver<T, Session>::unpacker_type>
    >::make();

template<class T, class Session>
const std::array<
    typename receiver<io::streaming_tag<T>, Session>::unpacker_type,
    boost::mpl::size<typename receiver<io::streaming_tag<T>, Session>::result_type::types>::value
> receiver<io::streaming_tag<T>, Session>::unpackers =
    meta::to_array<
        typename receiver<io::streaming_tag<T>, Session>::result_type::types,
        detail::unpacker_factory<Session, typename receiver<io::streaming_tag<T>, Session>::unpacker_type>
    >::make();

} // namespace framework

} // namespace cocaine
