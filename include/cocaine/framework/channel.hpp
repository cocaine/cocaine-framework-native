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

#include <tuple>

#include <cocaine/rpc/protocol.hpp>

#include <cocaine/framework/forwards.hpp>

namespace cocaine {

namespace framework {

/// The channel class represents a named tuple channel.
template<class Event>
struct channel {
    typedef sender  <typename io::event_traits<Event>::dispatch_type, basic_session_t> sender_type;
    typedef receiver<typename io::event_traits<Event>::upstream_type, basic_session_t> receiver_type;
    typedef std::tuple<sender_type, receiver_type> tuple_type;

    sender_type tx;
    receiver_type rx;

    explicit channel(tuple_type&& tuple) :
        tx(std::move(std::get<0>(tuple))),
        rx(std::move(std::get<1>(tuple)))
    {}
};

} // namespace framework

} // namespace cocaine
