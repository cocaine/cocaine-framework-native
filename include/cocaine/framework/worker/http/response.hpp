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

#include <string>
#include <vector>

#include <cocaine/traits/tuple.hpp>

namespace cocaine {

namespace framework {

namespace worker {

struct http_response_t {
    int code;
    std::vector<std::pair<std::string, std::string>> headers;
};

} // namespace worker

} // namespace framework

namespace io {

template<>
struct type_traits<framework::worker::http_response_t> {
    typedef boost::mpl::list<
        int,
        std::vector<std::pair<std::string, std::string>>
    > tuple_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const framework::worker::http_response_t& source) {
        io::type_traits<
            tuple_type
        >::pack(packer, source.code, source.headers);
    }

    static inline
    void
    unpack(const msgpack::object& unpacked, framework::worker::http_response_t& target) {
        io::type_traits<
            tuple_type
        >::unpack(unpacked, target.code, target.headers);
    }
};

} // namespace io

} // namespace cocaine
