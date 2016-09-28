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
#include <boost/optional.hpp>

#include <cocaine/hpack/header.hpp>

namespace msgpack { struct object; }

namespace cocaine {
namespace framework {

/// The decoded message class represents movable unpacked MessagePack payload with internal storage.
class decoded_message {
    class inner_t;
    std::unique_ptr<inner_t> d;

public:
    /// Constructs a null-initialized message object.
    explicit decoded_message(boost::none_t);

    /// Constructs a message object from msgpack object, which data is stored in 'storage' buffer
    /// and header vector, which data is also owned by buffer.
    ///
    /// \pre object should represent valid MessagePack'ed Cocaine message, otherwise the behavior is
    /// undefined.
    decoded_message(msgpack::object, std::vector<char>&& storage, std::vector<hpack::header_t> headers);

    ~decoded_message();

    // TODO: Noexcept?
    decoded_message(decoded_message&& other);
    decoded_message& operator=(decoded_message&& other);

    /// Returns the message span id.
    auto span() const -> std::uint64_t;

    /// Returns the message type.
    auto type() const -> std::uint64_t;

    /// Returns the object representation of message arguments.
    auto args() const -> const msgpack::object&;

    auto meta() const noexcept -> const std::vector<hpack::header_t>&;

    template<class Header>
    boost::optional<hpack::header_t>
    get_header() const {
        auto header = hpack::header::find_first(meta(), Header::name());
        return header ? boost::make_optional(*header) : boost::none;
    }

};

} // namespace framework
} // namespace cocaine
