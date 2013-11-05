/*
Copyright (c) 2013 Andrey Goryachev <andrey.goryachev@gmail.com>
Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_FRAMEWORK_UPSTREAM_HPP
#define COCAINE_FRAMEWORK_UPSTREAM_HPP

#include <cocaine/api/stream.hpp>
#include <cocaine/traits.hpp>

namespace cocaine { namespace framework {

class upstream_t:
    public cocaine::api::stream_t
{
public:
    upstream_t()
    {
        // pass
    }

    virtual
    ~upstream_t() {
        // pass
    }

    virtual
    bool
    closed() const = 0;

    using cocaine::api::stream_t::write;

    template<class T>
    void
    write(const T& obj) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> packer(buffer);
        cocaine::io::type_traits<T>::pack(packer, obj);
        write(buffer.data(), buffer.size());
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_UPSTREAM_HPP
