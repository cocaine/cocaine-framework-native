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

#include <cstdint>
#include <memory>
#include <stddef.h>

#include <boost/none_t.hpp>

namespace msgpack { struct object; }

namespace cocaine { namespace framework {

/// The decoded message class represents movable unpacked MessagePack payload with internal storage.
class decoded_message {
    class impl;
    std::unique_ptr<impl> d;

public:
    /// Constructs a null-initialized message object.
    explicit decoded_message(boost::none_t);

    /// Constructs a message object from its raw parts.
    ///
    /// \pre the message should be valid MessagePack'ed Cocaine message, otherwise the behavior is
    /// undefined.
    /// \throws std::bad_alloc if unable to allocate the required memory chunk.
    decoded_message(const char* data, std::size_t size);

    ~decoded_message();

    decoded_message(decoded_message&& other);
    decoded_message& operator=(decoded_message&& other);

    /// Returns the message span id.
    auto span() const -> std::uint64_t;

    /// Returns the message type.
    auto type() const -> std::uint64_t;

    /// Returns the object representation of message arguments.
    auto args() const -> const msgpack::object&;
};

}} // namespace cocaine::framework
