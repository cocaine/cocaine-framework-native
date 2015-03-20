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

#include <cocaine/rpc/protocol.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

/// The invocation result helper trait allows you to unpack invocation result to something more
/// convenient.
///
/// Must provide the type typedef for result type.
///
/// \helper
template<
    class Event,
    class Upstream = typename io::event_traits<Event>::upstream_type,
    class Dispatch = typename io::event_traits<Event>::dispatch_type
>
struct invocation_result {
    typedef channel<Event> channel_type;
    typedef channel_type type;

    static
    typename task<type>::future_type
    apply(channel_type&& channel) {
        return make_ready_future<type>::value(std::move(channel));
    }
};

/// The template trait specialization for primitive tags with no sender.
///
/// On apply unpacks the single option value and returns it on value type, throws otherwise.
///
/// \helper
template<class Event, class T>
struct invocation_result<Event, io::primitive_tag<T>, void> {
    typedef channel<Event> channel_type;
    typedef typename detail::packable<T>::type type;

    static
    typename task<type>::future_type
    apply(channel_type&& channel) {
        return channel.rx.recv();
    }
};

} // namespace framework

} // namespace cocaine
