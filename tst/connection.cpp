#include <thread>

#include <gtest/gtest.h>

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
        client.loop().post([&client, &flag, &endpoint]{
            auto conn = std::make_shared<basic_connection_t>(client.loop());
            EXPECT_NO_THROW(conn->connect(endpoint, [&flag, &conn](const std::error_code& ec) {
                EXPECT_TRUE(conn->connected());
                EXPECT_EQ(0, ec.value());
                flag = true;
            }));
        });
    }

    EXPECT_TRUE(flag);
}

TEST(basic_connection_t, ConnectMultipleTimesResultsInError) {
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

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
        auto conn = std::make_shared<basic_connection_t>(client.loop());
        conn->connect(endpoint, [&flag, &conn](const std::error_code& ec) {
            EXPECT_TRUE(conn->connected());
            EXPECT_EQ(0, ec.value());
            flag++;
        });

        conn->connect(endpoint, [&flag, &conn](const std::error_code& ec) {
            EXPECT_FALSE(conn->connected());
            EXPECT_EQ(io::error::in_progress, ec);
            flag++;
        });
    }

    EXPECT_EQ(2, flag);
}

TEST(basic_channel_t, ConnectAfterConnectedResultsInError) {
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

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
        auto conn = std::make_shared<basic_connection_t>(client.loop());
        conn->connect(endpoint, [&flag, &conn, &endpoint](const std::error_code& ec) {
            EXPECT_TRUE(conn->connected());
            EXPECT_EQ(0, ec.value());
            flag++;

            conn->connect(endpoint, [&flag, &conn](const std::error_code& ec) {
                EXPECT_TRUE(conn->connected());
                EXPECT_EQ(io::error::already_connected, ec);
                flag++;
            });
        });
    }

    EXPECT_EQ(2, flag);
}
