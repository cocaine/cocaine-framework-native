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

#include "cocaine/framework/detail/decoder.hpp"

#include <vector>

#include <msgpack.hpp>

#include <cocaine/common.hpp>
#include <cocaine/rpc/asio/decoder.hpp>

#include "cocaine/framework/message.hpp"

using namespace cocaine::framework::detail;

size_t decoder_t::decode(const char* data, size_t size, message_type& message, std::error_code& ec) {
    size_t offset = 0;

    msgpack::object object;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    std::auto_ptr<msgpack::zone> zone(new msgpack::zone);
#pragma GCC diagnostic pop
    msgpack::unpack_return rv = msgpack::unpack(data, size, &offset, zone.get(), &object);

    if(rv == msgpack::UNPACK_SUCCESS || rv == msgpack::UNPACK_EXTRA_BYTES) {
        if (object.type != msgpack::type::ARRAY || object.via.array.size < 3) {
            ec = error::frame_format_error;
        } else if(object.via.array.ptr[0].type != msgpack::type::POSITIVE_INTEGER ||
                  object.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER ||
                  object.via.array.ptr[2].type != msgpack::type::ARRAY)
        {
            ec = error::frame_format_error;
        } else {
            message = message_type(data, offset);
        }
    } else if(rv == msgpack::UNPACK_CONTINUE) {
        ec = error::insufficient_bytes;
    } else if(rv == msgpack::UNPACK_PARSE_ERROR) {
        ec = error::parse_error;
    }

    return offset;
}
