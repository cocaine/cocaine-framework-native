/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
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


#ifndef ENCODER_HPP
#define ENCODER_HPP

#include <cocaine/rpc/asio/encoder.hpp>

namespace cocaine { namespace framework {

typedef std::function<io::encoder_t::message_type(std::uint64_t, io::encoder_t&)> encode_callback_t;
typedef std::function<io::encoder_t::message_type(io::encoder_t&)> bound_encode_callback_t;

template<class Event, class... Args>
static
io::encoder_t::message_type
encode(std::uint64_t span, io::encoder_t& encoder, Args&... args) {
    return encoder.encode<Event>(span, std::forward<Args>(args)...);
}

}}
#endif // ENCODER_HPP

