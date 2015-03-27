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

#include <boost/none_t.hpp>

namespace msgpack { struct object; }

namespace cocaine {

namespace framework {

class decoded_message {
    class impl;
    std::unique_ptr<impl> d;

public:
    explicit decoded_message(boost::none_t);
    decoded_message(const char* data, size_t size);
    ~decoded_message();

    decoded_message(decoded_message&& other);
    decoded_message& operator=(decoded_message&& other);

    auto span() const -> uint64_t;
    auto type() const -> uint64_t;
    auto args() const -> const msgpack::object&;
};

} // namespace framework

} // namespace cocaine
