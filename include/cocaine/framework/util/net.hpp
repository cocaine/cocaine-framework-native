#pragma once

#include <asio/ip/tcp.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace cocaine {

namespace framework {

namespace util {

boost::asio::ip::address address_cast(const asio::ip::address& address);
asio::ip::address address_cast(const boost::asio::ip::address& address);

boost::asio::ip::tcp::endpoint endpoint_cast(const asio::ip::tcp::endpoint& endpoint);
asio::ip::tcp::endpoint endpoint_cast(const boost::asio::ip::tcp::endpoint& endpoint);

}

}

}
