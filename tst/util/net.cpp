#include "net.hpp"

#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>

std::uint16_t testing::util::port() {
    io::io_service loop;
    io::ip::tcp::acceptor acceptor(loop);
    io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    acceptor.listen();
    return acceptor.local_endpoint().port();
}
