#include "net.hpp"

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

std::uint16_t testing::util::port() {
    boost::asio::io_service loop;
    boost::asio::ip::tcp::acceptor acceptor(loop);
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    acceptor.listen();
    return acceptor.local_endpoint().port();
}
