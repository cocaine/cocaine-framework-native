#ifndef COCAINE_FRAMEWORK_FUTURE_TRAITS_HPP
#define COCAINE_FRAMEWORK_FUTURE_TRAITS_HPP

#include <tuple>
#include <functional>

namespace cocaine { namespace framework {

typedef std::function<void(std::function<void()>)> executor_t;

namespace detail { namespace future {

template<class... Args>
struct storable_type {
    typedef std::tuple<Args...> type;
};

template<class Arg>
struct storable_type<Arg> {
    typedef Arg type;
};

template<class Arg>
struct storable_type<Arg&> {
    typedef std::reference_wrapper<Arg> type;
};

template<>
struct storable_type<void> {
    typedef std::tuple<> type;
};

}} // namespace detail::future

template<class... Args>
struct future_traits {
    typedef typename detail::future::storable_type<Args...>::type
            storable_type;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_FUTURE_TRAITS_HPP
