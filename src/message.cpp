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

using namespace cocaine::framework;

class decoded_message::impl {
public:
    impl() {}

    impl(const char* data, std::size_t size) {
        storage.resize(size);
        std::copy(data, data + size, storage.begin());

        size_t offset = 0;
        msgpack::object object;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        std::auto_ptr<msgpack::zone> zone(new msgpack::zone);
#pragma GCC diagnostic pop
        msgpack::unpack(storage.data(), storage.size(), &offset, zone.get(), &object);

        unpacked.zone() = zone;
        unpacked.get() = object;
    }

    std::vector<char> storage;
    msgpack::unpacked unpacked;
};

decoded_message::decoded_message(boost::none_t) :
    d(new impl)
{}

decoded_message::decoded_message(const char* data, std::size_t size) :
    d(new impl(data, size))
{}

decoded_message::~decoded_message() {}

decoded_message::decoded_message(decoded_message&& other) :
    d(std::move(other.d))
{}

decoded_message& decoded_message::operator=(decoded_message&& other) {
    d = std::move(other.d);
    return *this;
}

auto decoded_message::span() const -> std::uint64_t {
    return d->unpacked.get().via.array.ptr[0].as<std::uint64_t>();
}

auto decoded_message::type() const -> std::uint64_t {
    return d->unpacked.get().via.array.ptr[1].as<std::uint64_t>();
}

auto decoded_message::args() const -> const msgpack::object& {
    return d->unpacked.get().via.array.ptr[2];
}
