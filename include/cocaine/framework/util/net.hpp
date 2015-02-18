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

template<class To, class From>
std::vector<To> endpoints_cast(const std::vector<From>& from) {
    std::vector<To> result;
    for (auto it = from.begin(); it != from.end(); ++it) {
        result.push_back(endpoint_cast(*it));
    }

    return result;
}

}

}

}
