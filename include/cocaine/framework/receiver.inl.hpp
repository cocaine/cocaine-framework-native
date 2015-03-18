#pragma once

#include <boost/mpl/front.hpp>
#include <boost/mpl/size.hpp>

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

} // namespace detail

} // namespace framework

} // namespace cocaine
