#pragma once

#include <array>
#include <cstdint>

#include <boost/mpl/at.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/size.hpp>

#include <boost/variant.hpp>

#include <cocaine/rpc/protocol.hpp>
#include <cocaine/tuple.hpp>
#include <cocaine/utility.hpp>

#include <cocaine/framework/forwards.hpp>

namespace cocaine {

namespace framework {

namespace detail {

/// Transforms a typelist sequence into a single movable argument type.
///
/// If the sequence contains a single element of type T, then the result type will be T.
/// Otherwise the sequence will be transformed into a tuple.
///
/// packable<sequence<T, U...>>::type -> std::tuple<T, U...>,
/// packable<sequence<T>>::type       -> T.
///
/// \internal
template<class U, size_t = boost::mpl::size<U>::value>
struct packable {
    typedef typename tuple::fold<U>::type type;
};

/// The template specialization for single-element typelists.
///
/// \internal
template<class U>
struct packable<U, 1> {
    typedef typename boost::mpl::front<U>::type type;
};

/// Transforms event argument types (which is usually boost::mpl sequence) into a tuple.
///
/// into_tuple<primitive_tag<T>>::type -> std::tuple<T>
///
/// \internal
template<class Event>
struct into_tuple {
    typedef typename tuple::fold<
        typename io::event_traits<Event>::argument_type
    >::type type;
};

/// Transforms tag type into a variant with all possible tuple results.
///
/// variant_of<primitive_tag<T>>::type ->
///     variant<std::tuple<T>, std::tuple<int, std::string>>
///
/// variant_of<streaming_tag<T>>::type ->
///     variant<std::tuple<T>, std::tuple<int, std::string>, std::tuple<>>
///
/// \internal
template<class T>
struct variant_of {
    typedef typename boost::make_variant_over<
        typename boost::mpl::transform<
            typename io::protocol<T>::messages,
            into_tuple<boost::mpl::_1>
        >::type
    >::type type;
};

/// Transforms a receiver type into a variant with all possible pairs of produced receivers with
/// the result of previous recv() operation.
///
/// \internal
template<class Receiver>
struct result_of {
private:
    typedef Receiver receiver_type;

    /// Provides information about one of possible type, which can be returned by a receiver with a
    /// generic tag, if the received message has Event type.
    ///
    /// Returns a tuple containing the next receiver and the current result in the common case.
    /// Returns a tuple containing result types if there are no possible to receive messages after that.
    ///
    /// Do not use this trait with top-level event type.
    ///
    /// \internal
    template<class Event>
    struct result_of_event {
    private:
        typedef typename io::event_traits<Event>::argument_type argument_type;
        typedef typename io::event_traits<Event>::dispatch_type dispatch_type;

        typedef typename tuple::fold<argument_type>::type tuple_type;

    public:
        typedef typename std::conditional<
            std::is_same<dispatch_type, void>::value,
            tuple_type,
            std::tuple<receiver<dispatch_type, typename receiver_type::session_type>, tuple_type>
        >::type type;
    };

    typedef typename boost::mpl::transform<
        typename io::protocol<typename receiver_type::tag_type>::messages,
        result_of_event<boost::mpl::_1>
    >::type typelist;

public:
    typedef typename boost::make_variant_over<typelist>::type type;
};

/// Performs MessagePack'ed objects unpacking into the expected receiver's result tuples.
///
/// \internal
template<typename, class>
struct unpacker;

/// Unpacker template specialization for unpacking MessagePack'ed results for generic receivers.
///
/// \internal
template<typename Tag, class Session, typename... Args>
struct unpacker<std::tuple<receiver<Tag, Session>, std::tuple<Args...>>, Session> {
    typedef std::tuple<receiver<Tag, Session>, std::tuple<Args...>> result_type;

    inline
    std::tuple<receiver<Tag, Session>, std::tuple<Args...>>
    operator()(std::shared_ptr<basic_receiver_t<Session>> d, const msgpack::object& message) {
        unpacker<std::tuple<Args...>, Session> up;
        return std::make_tuple(std::move(d), up(message));
    }
};

/// Unpacker template specialization for unpacking MessagePack'ed results for terminal receivers.
///
/// In this case there are no possible next receivers available.
///
/// \internal
template<class Session, typename... Args>
struct unpacker<std::tuple<Args...>, Session> {
    typedef std::tuple<Args...> result_type;

    inline
    std::tuple<Args...>
    operator()(std::shared_ptr<basic_receiver_t<Session>>, const msgpack::object& message) {
        unpacker<std::tuple<Args...>, Session> up;
        return up(message);
    }

    inline
    std::tuple<Args...>
    operator()(const msgpack::object& message) {
        std::tuple<Args...> result;
        io::type_traits<std::tuple<Args...>>::unpack(message, result);
        return result;
    }
};

} // namespace detail

namespace meta {

template<class Sequence, class F>
struct to_array {
    typedef Sequence sequence_type;
    typedef typename F::result_type value_type;
    static constexpr std::size_t size = boost::mpl::size<sequence_type>::value;

    typedef std::array<value_type, size> result_type;

private:
    template<class IndexSequence>
    struct helper;

    template<size_t... Index>
    struct helper<index_sequence<Index...>> {
        static inline
        result_type
        apply() {
            return result_type {{
                F::template apply<typename boost::mpl::at<sequence_type, boost::mpl::int_<Index>>::type>()...
            }};
        }
    };

public:
    static constexpr
    result_type make() {
        return helper<typename make_index_sequence<size>::type>::apply();
    }
};

} // namespace meta

} // namespace framework

} // namespace cocaine
