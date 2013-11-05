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

#ifndef COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP
#define COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP

#include <cocaine/framework/service.hpp>
#include <cocaine/traits/literal.hpp>

namespace cocaine { namespace framework {

struct storage_service_t :
    public service_t
{
    static const unsigned int version = cocaine::io::protocol<cocaine::io::storage_tag>::version::value;

    storage_service_t(std::shared_ptr<service_connection_t> connection) :
        service_t(connection)
    {
        // pass
    }

    service_traits<cocaine::io::storage::read>::future_type
    read(const std::string& collection,
         const std::string& key)
    {
        return call<cocaine::io::storage::read>(collection, key);
    }

    template<class Value>
    service_traits<cocaine::io::storage::write>::future_type
    write(const std::string& collection,
          const std::string& key,
          Value&& value)
    {
        return call<cocaine::io::storage::write>(collection, key, std::forward<Value>(value));
    }

    template<class Value>
    service_traits<cocaine::io::storage::write>::future_type
    write(const std::string& collection,
          const std::string& key,
          Value&& value,
          const std::vector<std::string>& tags)
    {
        return call<cocaine::io::storage::write>(collection, key, std::forward<Value>(value), tags);
    }

    service_traits<cocaine::io::storage::remove>::future_type
    remove(const std::string& collection,
           const std::string& key)
    {
        return call<cocaine::io::storage::remove>(collection, key);
    }

    service_traits<cocaine::io::storage::find>::future_type
    find(const std::string& collection,
         const std::vector<std::string>& tags)
    {
        return call<cocaine::io::storage::find>(collection, tags);
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP
