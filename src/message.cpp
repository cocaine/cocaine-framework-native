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

#include "cocaine/framework/message.hpp"

#include <algorithm>
#include <type_traits>
#include <vector>

#include <msgpack/object.hpp>
#include <msgpack/type/int.hpp>
#include <msgpack/unpack.hpp>
#include <msgpack/zone.hpp>

#include <cocaine/rpc/asio/header.hpp>
using namespace cocaine::framework;

class decoded_message::impl {
public:
    impl() {}

    impl(msgpack::object _obj, std::vector<char> _storage, std::vector<io::header_t> _headers) :
        obj(std::move(_obj)),
        storage(std::move(_storage)),
        headers(std::move(_headers))
    {}

    msgpack::object obj;
    std::vector<char> storage;
    std::vector<io::header_t> headers;
};

decoded_message::decoded_message(boost::none_t) :
    d(new impl)
{}

decoded_message::decoded_message(msgpack::object obj, std::vector<char> storage, std::vector<io::header_t> headers) :
    d(new impl(std::move(obj), std::move(storage), std::move(headers)))
{}

decoded_message::~decoded_message() {}

decoded_message::decoded_message(decoded_message&& other) :
    d(std::move(other.d))
{}

decoded_message& decoded_message::operator=(decoded_message&& other) {
    d = std::move(other.d);
    return *this;
}

auto decoded_message::span() const -> uint64_t {
    return d->obj.via.array.ptr[0].as<uint64_t>();
}

auto decoded_message::type() const -> uint64_t {
    return d->obj.via.array.ptr[1].as<uint64_t>();
}

auto decoded_message::args() const -> const msgpack::object& {
    return d->obj.via.array.ptr[2];
}

auto decoded_message::get_header(const io::header_key_t& key) const -> boost::optional<io::header_t> {
    for(auto& header : d->headers) {
        if(header.get_name() == key) {
            return boost::make_optional(header);
        }
    }
    return boost::none;
}
