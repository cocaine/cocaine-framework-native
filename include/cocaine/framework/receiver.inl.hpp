#pragma once

#include <boost/mpl/front.hpp>
#include <boost/mpl/size.hpp>

#include <boost/variant.hpp>

#include <cocaine/rpc/protocol.hpp>
#include <cocaine/tuple.hpp>

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

} // namespace detail

} // namespace framework

} // namespace cocaine
