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

#include "cocaine/framework/detail/net.hpp"

#include <stdexcept>

#include <boost/version.hpp>

#if BOOST_VERSION < 104800
#include <array>
#include <boost/array.hpp>
#endif

namespace cocaine { namespace framework { namespace detail {

boost::asio::ip::address
address_cast(const asio::ip::address& address) {
    if (address.is_v4()) {
        return boost::asio::ip::address_v4(address.to_v4().to_ulong());
    } else if (address.is_v6()) {
#if BOOST_VERSION >= 104800
        return boost::asio::ip::address_v6(address.to_v6().to_bytes());
#else
        auto from = address.to_v6().to_bytes();
        boost::array<unsigned char, 16> bytes;
        std::copy(from.begin(), from.end(), bytes.begin());
        return boost::asio::ip::address_v6(bytes);
#endif
    }

    throw std::invalid_argument("address must be either v4 or v6");
}

asio::ip::address
address_cast(const boost::asio::ip::address& address) {
    if (address.is_v4()) {
        return asio::ip::address_v4(address.to_v4().to_ulong());
    } else if (address.is_v6()) {
#if BOOST_VERSION >= 104800
        return asio::ip::address_v6(address.to_v6().to_bytes());
#else
        auto from = address.to_v6().to_bytes();
        std::array<unsigned char, 16> bytes;
        std::copy(from.begin(), from.end(), bytes.begin());
        return asio::ip::address_v6(bytes);
#endif
    }

    throw std::invalid_argument("address must be either v4 or v6");
}

boost::asio::ip::tcp::endpoint
endpoint_cast(const asio::ip::tcp::endpoint& endpoint) {
    return boost::asio::ip::tcp::endpoint(
        address_cast(endpoint.address()), endpoint.port()
    );
}

asio::ip::tcp::endpoint
endpoint_cast(const boost::asio::ip::tcp::endpoint& endpoint) {
    return asio::ip::tcp::endpoint(
        address_cast(endpoint.address()), endpoint.port()
    );
}

}}} // namespace cocaine::framework::detail
