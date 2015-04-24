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

#include <vector>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>

namespace cocaine { namespace framework { namespace detail {

boost::asio::ip::address address_cast(const asio::ip::address& address);
asio::ip::address address_cast(const boost::asio::ip::address& address);

boost::asio::ip::tcp::endpoint endpoint_cast(const asio::ip::tcp::endpoint& endpoint);
asio::ip::tcp::endpoint endpoint_cast(const boost::asio::ip::tcp::endpoint& endpoint);

template<class To, class From>
std::vector<To> endpoints_cast(const std::vector<From>& from) {
    std::vector<To> result;
    for (auto it = from.begin(); it != from.end(); ++it) {
        result.push_back(endpoint_cast(*it));
    }

    return result;
}

}}} // namespace cocaine::framework::detail
