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

#include <stddef.h>
#include <system_error>

#include <hpack-headers/header.hpp>

#include <msgpack.hpp>
namespace cocaine { namespace framework {

class decoded_message;

namespace detail {

/// The decoder represents streaming MessagePack decoding.
///
/// \note this class does explicit memory copying to the message_type object.
/// \internal
struct decoder_t {
    typedef decoded_message message_type;
    hpack::header_table_t header_table;
    msgpack::zone zone;

    size_t decode(const char* data, size_t size, message_type& message, std::error_code& ec);
};

} // namespace detail

}} // namespace cocaine::framework
