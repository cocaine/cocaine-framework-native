/*
Copyright (c) 2013 Andrey Goryachev <andrey.goryachev@gmail.com>
Copyright (c) 2013 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_FRAMEWORK_SERVICES_APP_HPP
#define COCAINE_FRAMEWORK_SERVICES_APP_HPP

#include <cocaine/framework/service.hpp>

#include <cocaine/traits/literal.hpp>

namespace cocaine { namespace framework {

struct app_service_t :
    public service_t
{
    static const unsigned int version = io::protocol<io::app_tag>::version::value;

    app_service_t(std::shared_ptr<service_connection_t> connection) :
        service_t(connection)
    {
        // Pass.
    }

    template<class T>
    service_traits<io::app::enqueue>::future_type
    enqueue(const std::string& event, const T& chunk) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> packer(buffer);

        io::type_traits<T>::pack(packer, chunk);

        return call<io::app::enqueue>(event, io::literal { buffer.data(), buffer.size() });
    }

    template<class T>
    service_traits<io::app::enqueue>::future_type
    enqueue(const std::string& event, const T& chunk, const std::string& tag) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> packer(buffer);

        io::type_traits<T>::pack(packer, chunk);

        return call<io::app::enqueue>(event, io::literal { buffer.data(), buffer.size() }, tag);
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_APP_HPP
