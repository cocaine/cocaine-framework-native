/*
Copyright (c) 2013 Andrey Goryachev <andrey.goryachev@gmail.com>
Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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
