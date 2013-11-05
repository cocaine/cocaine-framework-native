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
