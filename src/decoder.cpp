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

#include <memory>

#include <hpack-headers/header.hpp>
#include <hpack-headers/msgpack_traits.hpp>

#include <msgpack/object.hpp>
#include <msgpack/unpack.hpp>
#include <msgpack/zone.hpp>

#include <cocaine/common.hpp>
#include <cocaine/errors.hpp>

#include <cocaine/rpc/asio/decoder.hpp>

#include "cocaine/framework/message.hpp"

using namespace cocaine::framework::detail;

size_t decoder_t::decode(const char* data, size_t size, message_type& message, std::error_code& ec) {
    size_t offset = 0;

    msgpack::object object;
    std::vector<char> buffer(size);
    memcpy(buffer.data(), data, size);
    msgpack::unpack_return rv = msgpack::unpack(buffer.data(), size, &offset, &zone, &object);

    if(rv == msgpack::UNPACK_SUCCESS || rv == msgpack::UNPACK_EXTRA_BYTES) {
        std::vector<hpack::header_t> headers;
        bool error = false;
        error = error || object.type != msgpack::type::ARRAY;
        error = error || object.via.array.size < 3;
        error = error || object.via.array.ptr[0].type != msgpack::type::POSITIVE_INTEGER;
        error = error || object.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER;
        error = error || object.via.array.ptr[2].type != msgpack::type::ARRAY;
        if(object.via.array.size > 3) {
            error = error || object.via.array.ptr[3].type != msgpack::type::ARRAY;
            error = error || hpack::msgpack_traits::unpack_vector(object.via.array.ptr[3], header_table, headers);
        }
        if(error) {
            ec = error::frame_format_error;
        }
        message = message_type(std::move(object), std::move(buffer), std::move(headers));
    } else if(rv == msgpack::UNPACK_CONTINUE) {
        ec = error::insufficient_bytes;
    } else if(rv == msgpack::UNPACK_PARSE_ERROR) {
        ec = error::parse_error;
    }

    return offset;
}
