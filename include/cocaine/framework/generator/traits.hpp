#ifndef COCAINE_FRAMEWORK_GENERATOR_TRAITS_HPP
#define COCAINE_FRAMEWORK_GENERATOR_TRAITS_HPP

#include <cocaine/framework/basic_stream.hpp>
#include <cocaine/framework/future.hpp>

namespace cocaine { namespace framework {

template<class... Args>
struct generator_traits {
    // type that returns method 'next()' of generator
    typedef typename future_traits<Args...>::storable_type
            single_type;

    // stream<Args...> is derived from this basic_stream
    typedef basic_stream<single_type>
            stream_type;
};

template<>
struct generator_traits<void> {
    typedef void
            single_type;

    typedef basic_stream<single_type>
            stream_type;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_GENERATOR_TRAITS_HPP
