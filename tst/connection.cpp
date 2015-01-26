#include <thread>

#include <gtest/gtest.h>

#include <cocaine/common.hpp>
#include <cocaine/rpc/asio/encoder.hpp>

#include <cocaine/framework/connection.hpp>

#include "util/net.hpp"

using namespace cocaine::framework;
using namespace testing::util;

TEST(basic_connection_t, Constructor) {
    loop_t loop;
    auto conn = std::make_shared<basic_connection_t>(loop);
    EXPECT_FALSE(conn->connected());
}

TEST(basic_connection_t, Connect) {
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        io::deadline_timer timer(loop);
        timer.expires_from_now(boost::posix_time::milliseconds(testing::util::TIMEOUT));
        timer.async_wait([&acceptor](const std::error_code& ec){
            EXPECT_EQ(io::error::operation_aborted, ec);
            acceptor.cancel();
        });

        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&timer](const std::error_code&){
            timer.cancel();
        });

        EXPECT_NO_THROW(loop.run());
    });

    std::atomic<bool> flag(false);

    {
        client_t client;

        // ===== Test Stage =====
        client.loop().post([&client, &flag, endpoint]{
            auto conn = std::make_shared<basic_connection_t>(client.loop());
            conn->connect(endpoint, [&flag, conn](const std::error_code& ec) {
                EXPECT_EQ(0, ec.value());
                EXPECT_TRUE(conn->connected());
                flag = true;
            });
        });
    }

    EXPECT_TRUE(flag);
}

TEST(basic_connection_t, ConnectMultipleTimesResultsInError) {
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    boost::barrier barrier(2);
    server_t server(port, [&barrier](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        barrier.wait();

        io::deadline_timer timer(loop);
        timer.expires_from_now(boost::posix_time::milliseconds(testing::util::TIMEOUT));
        timer.async_wait([&acceptor](const std::error_code& ec){
            EXPECT_EQ(io::error::operation_aborted, ec);
            acceptor.cancel();
        });

        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&timer](const std::error_code&){
            timer.cancel();
        });

        EXPECT_NO_THROW(loop.run());
    });

    std::atomic<int> flag(0);

    {
        client_t client;

        // ===== Test Stage =====
        client.loop().post([&client, &flag, endpoint]{
            auto conn = std::make_shared<basic_connection_t>(client.loop());
            conn->connect(endpoint, [&flag, conn](const std::error_code& ec) {
                EXPECT_EQ(0, ec.value());
                EXPECT_TRUE(conn->connected());
                flag++;
            });

            conn->connect(endpoint, [&flag, conn](const std::error_code& ec) {
                EXPECT_EQ(io::error::in_progress, ec);
                flag++;
            });
        });

        barrier.wait();
    }

    EXPECT_EQ(2, flag);
}

TEST(basic_connection_t, ConnectAfterConnectedResultsInError) {
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        io::deadline_timer timer(loop);
        timer.expires_from_now(boost::posix_time::milliseconds(testing::util::TIMEOUT));
        timer.async_wait([&acceptor](const std::error_code& ec){
            EXPECT_EQ(io::error::operation_aborted, ec);
            acceptor.cancel();
        });

        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&timer](const std::error_code&){
            timer.cancel();
        });

        EXPECT_NO_THROW(loop.run());
    });

    std::atomic<int> flag(0);

    {
        client_t client;

        // ===== Test Stage =====
        client.loop().post([&client, &flag, endpoint]{
            auto conn = std::make_shared<basic_connection_t>(client.loop());
            conn->connect(endpoint, [&flag, conn, &endpoint](const std::error_code& ec) {
                EXPECT_EQ(0, ec.value());
                EXPECT_TRUE(conn->connected());
                flag++;

                conn->connect(endpoint, [&flag, conn](const std::error_code& ec) {
                    EXPECT_TRUE(conn->connected());
                    EXPECT_EQ(io::error::already_connected, ec);
                    flag++;
                });
            });
        });
    }

    EXPECT_EQ(2, flag);
}

TEST(basic_connection_t, DisconnectWhileConnecting) {
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    std::atomic<bool> flag(false);

    {
        client_t client;

        // ===== Test Stage =====
        client.loop().post([&client, &flag, endpoint]{
            auto conn = std::make_shared<basic_connection_t>(client.loop());
            conn->connect(endpoint, [&flag, conn](const std::error_code& ec) {
                EXPECT_EQ(io::error::operation_aborted, ec);
                EXPECT_FALSE(conn->connected());
                flag = true;
            });

            conn->disconnect();
        });
    }

    EXPECT_TRUE(flag);
}

struct large_event;

namespace cocaine { namespace io {

template<>
struct event_traits<large_event> {
    enum constants { id = 0 };

    typedef std::vector<int> argument_type;
    typedef void dispatch_type;
    typedef void upstream_type;
};

}} // namespace cocaine::io

TEST(basic_connection_t, DisconnectWhileWriting) {
    // Can sometimes fail, because an OS can write entire buffer before disconnect event occurs.
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    static const int BUFFER_SIZE = 5 * 1024 * 1024;

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        io::deadline_timer timer(loop);
        timer.expires_from_now(boost::posix_time::milliseconds(testing::util::TIMEOUT));
        timer.async_wait([&acceptor](const std::error_code& ec){
            EXPECT_EQ(io::error::operation_aborted, ec);
            acceptor.cancel();
        });

        io::ip::tcp::socket socket(loop);
        std::vector<char> buffer(BUFFER_SIZE);
        acceptor.async_accept(socket, [&timer, &socket, &buffer](const std::error_code&){
            timer.cancel();
            timer.expires_from_now(boost::posix_time::milliseconds(testing::util::TIMEOUT));
            timer.async_wait([&socket](const std::error_code& ec){
                EXPECT_EQ(io::error::operation_aborted, ec);
                socket.cancel();
            });

            io::async_read(socket, io::buffer(buffer), [&timer](const std::error_code& ec, size_t s){
                EXPECT_EQ(io::error::eof, ec);
                timer.cancel();
            });
        });

        EXPECT_NO_THROW(loop.run());
    });

    std::atomic<bool> flag(false);

    std::vector<int> v(BUFFER_SIZE);
    cocaine::io::encoded<large_event> message(1, v);

    {
        client_t client;

        // ===== Test Stage =====
        client.loop().post([&client, &flag, endpoint, &message]{
            auto conn = std::make_shared<basic_connection_t>(client.loop());
            conn->connect(endpoint, [&flag, conn, &message](const std::error_code& ec) {
                EXPECT_EQ(0, ec.value());

                conn->write(message, [&flag, conn](const std::error_code& ec){
                    // We have closed the socket manually, so any further read/write on it should
                    // result in EBADF.
                    EXPECT_EQ(io::error::bad_descriptor, ec);
                    EXPECT_FALSE(conn->connected());
                    flag = true;
                });

                conn->disconnect();
            });
        });
    }

    EXPECT_TRUE(flag);
}
