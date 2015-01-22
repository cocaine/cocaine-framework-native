#include <type_traits>

#include <boost/thread.hpp>
#include <boost/thread/barrier.hpp>

#include <asio/write.hpp>

#include <gtest/gtest.h>

#include <cocaine/dynamic.hpp>
#include <cocaine/idl/locator.hpp>
#include <cocaine/idl/node.hpp>

#include <cocaine/framework/connection.hpp>

#include "util/net.hpp"

using namespace cocaine::framework;

TEST(Connection, Constructor) {
    loop_t loop;
    auto conn = std::make_shared<connection_t>(loop);

    EXPECT_FALSE(conn->connected());

    static_assert(std::is_nothrow_constructible<connection_t, loop_t&>::value, "must be noexcept");
}

TEST(Connection, Connect) {
    // ===== Set Up Stage =====
    // We create a TCP server in separate thread and wait for incoming connection. After accepting
    // just close the socket.
    // Note, that some rendezvous point is required to be sure, that the server has been started
    // when the client is trying to connect.

    // An OS should select available port for us.
    const std::uint16_t port = testing::util::port();
    boost::barrier barrier(2);
    boost::thread server_thread([port, &barrier]{
        loop_t loop;
        io::ip::tcp::acceptor acceptor(loop);
        io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

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


    loop_t loop;
    std::unique_ptr<loop_t::work> work(new loop_t::work(loop));
    boost::thread client_thread([&loop]{
        EXPECT_NO_THROW(loop.run());
    });

    // Here we wait until the server has been started.
    barrier.wait();

    // ===== Test Stage =====
    auto conn = std::make_shared<connection_t>(loop);

    io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
    EXPECT_NO_THROW(conn->connect(endpoint).get());
    EXPECT_TRUE(conn->connected());

    // ===== Tear Down Stage =====
    work.reset();
    client_thread.join();

    server_thread.join();
}

TEST(Connection, ConnectOnInvalidPort) {
    // ===== Set Up Stage =====
    loop_t loop;
    std::unique_ptr<loop_t::work> work(new loop_t::work(loop));
    boost::thread thread([&loop]{
        loop.run();
    });

    // ===== Test Stage =====
    auto conn = std::make_shared<connection_t>(loop);

    io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), 0);
    EXPECT_THROW(conn->connect(endpoint).get(), std::system_error);

    EXPECT_FALSE(conn->connected());

    // ===== Tear Down Stage =====
    work.reset();
    thread.join();
}

TEST(Connection, ConnectMultipleTimesOnDisconnectedService) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    boost::barrier barrier(2);
    boost::thread server_thread([port, &barrier]{
        loop_t loop;
        io::ip::tcp::acceptor acceptor(loop);
        io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

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

    loop_t client_loop;
    std::unique_ptr<loop_t::work> work(new loop_t::work(client_loop));
    boost::thread client_thread([&client_loop]{
        client_loop.run();
    });

    barrier.wait();

    // ===== Test Stage =====
    auto conn = std::make_shared<connection_t>(client_loop);

    io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
    auto f1 = conn->connect(endpoint).then([&conn](future_t<void>& f){
        EXPECT_NO_THROW(f.get());
        EXPECT_TRUE(conn->connected());
    });

    auto f2 = conn->connect(endpoint).then([&conn](future_t<void>& f){
        EXPECT_NO_THROW(f.get());
        EXPECT_TRUE(conn->connected());
    });

    f1.get();
    f2.get();

    // ===== Tear Down Stage =====
    work.reset();
    client_thread.join();

    server_thread.join();
}

TEST(Connection, ConnectOnConnectedService) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    boost::barrier barrier(2);
    boost::thread server_thread([port, &barrier]{
        loop_t loop;
        io::ip::tcp::acceptor acceptor(loop);
        io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

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

    loop_t loop;
    std::unique_ptr<loop_t::work> work(new loop_t::work(loop));
    boost::thread client_thread([&loop]{
        loop.run();
    });

    barrier.wait();

    // ===== Test Stage =====
    auto conn = std::make_shared<connection_t>(loop);

    io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
    EXPECT_NO_THROW(conn->connect(endpoint).get());
    EXPECT_TRUE(conn->connected());

    EXPECT_NO_THROW(conn->connect(endpoint).get());
    EXPECT_TRUE(conn->connected());

    // ===== Tear Down Stage =====
    work.reset();
    client_thread.join();

    server_thread.join();
}

TEST(Connection, RAIIOnConnect) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    boost::barrier barrier(2);
    boost::thread server_thread([port, &barrier]{
        loop_t loop;
        io::ip::tcp::acceptor acceptor(loop);
        io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

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

    loop_t loop;
    std::unique_ptr<loop_t::work> work(new loop_t::work(loop));
    boost::thread client_thread([&loop]{
        loop.run();
    });

    // Here we wait until the server has been started.
    barrier.wait();

    // ===== Test Stage =====
    future_t<void> future;
    {
        auto conn = std::make_shared<connection_t>(loop);

        io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
        future = std::move(conn->connect(endpoint));
    }
    EXPECT_NO_THROW(future.get());

    // ===== Tear Down Stage =====
    work.reset();
    client_thread.join();

    server_thread.join();
}

TEST(Encoder, InvokeEvent) {
    cocaine::io::encoded<cocaine::io::locator::resolve> message(1, std::string("node"));

    const std::vector<std::uint8_t> expected = {{ 147, 1, 0, 145, 164, 110, 111, 100, 101 }};

    EXPECT_EQ(expected, std::vector<std::uint8_t>(message.data(), message.data() + 9));
}

TEST(Connection, InvokeSendsProperMessage) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    boost::barrier barrier(2);
    boost::thread server_thread([port, &barrier]{
        loop_t loop;
        io::ip::tcp::acceptor acceptor(loop);
        io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

        barrier.wait();

        io::deadline_timer timer(loop);
        timer.expires_from_now(boost::posix_time::milliseconds(testing::util::TIMEOUT));
        timer.async_wait([&acceptor](const std::error_code& ec){
            EXPECT_EQ(io::error::operation_aborted, ec);
            acceptor.cancel();
        });

        std::array<std::uint8_t, 32> actual;
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&timer, &socket, &actual](const std::error_code&){
            timer.cancel();

            socket.async_read_some(io::buffer(actual), [&actual](const std::error_code& ec, size_t size){
                EXPECT_EQ(9, size);
                EXPECT_EQ(0, ec.value());

                std::array<std::uint8_t, 9> expected = {{ 147, 1, 0, 145, 164, 110, 111, 100, 101 }};
                for (int i = 0; i < 9; ++i) {
                    EXPECT_EQ(expected[i], actual[i]);
                }
            });
        });

        EXPECT_NO_THROW(loop.run());
    });

    loop_t loop;
    std::unique_ptr<loop_t::work> work(new loop_t::work(loop));
    boost::thread client_thread([&loop]{
        loop.run();
    });

    // Here we wait until the server has been started.
    barrier.wait();

    // ===== Test Stage =====
    auto conn = std::make_shared<connection_t>(loop);

    io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
    EXPECT_NO_THROW(conn->connect(endpoint).get());

    EXPECT_TRUE(conn->connected());
    conn->invoke<cocaine::io::locator::resolve>(std::string("node"));

    // ===== Tear Down Stage =====
    work.reset();
    client_thread.join();

    server_thread.join();
}

TEST(Connection, DecodeIncomingMessage) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    boost::barrier barrier(2);
    boost::thread server_thread([port, &barrier]{
        loop_t loop;
        io::ip::tcp::acceptor acceptor(loop);
        io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

        barrier.wait();

        io::deadline_timer timer(loop);
        timer.expires_from_now(boost::posix_time::milliseconds(testing::util::TIMEOUT));
        timer.async_wait([&acceptor](const std::error_code& ec){
            EXPECT_EQ(io::error::operation_aborted, ec);
            acceptor.cancel();
        });

        // The following sequence is an encoded [1, 0, [['echo', 'http']]] struct, which is the
        // real response from Node service's 'list' request.
        // The caller service must properly unpack it and return to the user.
        const std::array<std::uint8_t, 15> buf = {
            { 147, 1, 0, 145, 146, 164, 101, 99, 104, 111, 164, 104, 116, 116, 112 }
        };
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&timer, &socket, &buf](const std::error_code&){
            timer.cancel();

            // \note sometimes a race condition can occur - the socket should be read first.
            io::async_write(socket, io::buffer(buf), [](const std::error_code& ec, size_t size){
                EXPECT_EQ(0, ec.value());
                EXPECT_EQ(15, size);
            });
        });

        EXPECT_NO_THROW(loop.run());
    });

    loop_t loop;
    std::unique_ptr<loop_t::work> work(new loop_t::work(loop));
    boost::thread client_thread([&loop]{
        EXPECT_NO_THROW(loop.run());
    });

    // Here we wait until the server has been started.
    barrier.wait();

    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    // ===== Test Stage =====
    auto conn = std::make_shared<connection_t>(loop);
    conn->connect(endpoint).get();
    std::shared_ptr<receiver<cocaine::io::node::list>> rx;
    rx = conn->invoke<cocaine::io::node::list>();
    auto res = rx->recv().get();
    auto apps = boost::get<cocaine::dynamic_t>(res);
    EXPECT_EQ(cocaine::dynamic_t(std::vector<cocaine::dynamic_t>({ "echo", "http" })), apps);

    // ===== Tear Down Stage =====
    work.reset();
    client_thread.join();

    server_thread.join();
}

TEST(Connection, InvokeWhileServerClosesConnection) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    boost::barrier barrier(2);
    boost::thread server_thread([port, &barrier]{
        loop_t loop;
        io::ip::tcp::acceptor acceptor(loop);
        io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

        barrier.wait();

        io::deadline_timer timer(loop);
        timer.expires_from_now(boost::posix_time::milliseconds(testing::util::TIMEOUT));
        timer.async_wait([&acceptor](const std::error_code& ec){
            EXPECT_EQ(io::error::operation_aborted, ec);
            acceptor.cancel();
        });

        std::array<std::uint8_t, 32> actual;
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&timer, &socket, &actual](const std::error_code&){
            timer.cancel();

            socket.shutdown(asio::ip::tcp::socket::shutdown_both);
            socket.close();
        });

        EXPECT_NO_THROW(loop.run());
    });

    loop_t loop;
    std::unique_ptr<loop_t::work> work(new loop_t::work(loop));
    boost::thread client_thread([&loop]{
        loop.run();
    });

    // Here we wait until the server has been started.
    barrier.wait();

    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    // ===== Test Stage =====
    auto conn = std::make_shared<connection_t>(loop);
    conn->connect(endpoint).get();

    std::shared_ptr<receiver<cocaine::io::locator::resolve>> rx;
    rx = conn->invoke<cocaine::io::locator::resolve>(std::string("node"));
    EXPECT_THROW(rx->recv().get(), std::system_error);

    // ===== Tear Down Stage =====
    work.reset();
    client_thread.join();

    server_thread.join();
}

TEST(Connection, InvokeWhileConnectionResetByPeer) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    boost::barrier barrier(2);
    boost::thread server_thread([port, &barrier]{
        loop_t loop;
        io::ip::tcp::acceptor acceptor(loop);
        io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

        barrier.wait();

        io::deadline_timer timer(loop);
        timer.expires_from_now(boost::posix_time::milliseconds(testing::util::TIMEOUT));
        timer.async_wait([&acceptor](const std::error_code& ec){
            EXPECT_EQ(io::error::operation_aborted, ec);
            acceptor.cancel();
        });

        std::array<std::uint8_t, 32> actual;
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&timer, &socket, &actual](const std::error_code&){
            timer.cancel();

            socket.set_option(io::socket_base::linger(true, 0));
            socket.close();
        });

        EXPECT_NO_THROW(loop.run());
    });

    loop_t loop;
    std::unique_ptr<loop_t::work> work(new loop_t::work(loop));
    boost::thread client_thread([&loop]{
        loop.run();
    });

    // Here we wait until the server has been started.
    barrier.wait();

    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    // ===== Test Stage =====
    auto conn = std::make_shared<connection_t>(loop);
    conn->connect(endpoint).get();

    std::shared_ptr<receiver<cocaine::io::locator::resolve>> rx;
    rx = conn->invoke<cocaine::io::locator::resolve>(std::string("node"));
    EXPECT_THROW(rx->recv().get(), std::system_error);

    // ===== Tear Down Stage =====
    work.reset();
    client_thread.join();

    server_thread.join();
}

// Usage:
//  Service:
//    std::tie(tx, rx) = node.invoke<cocaine::io::node::list>(); // Nonblock, maybe noexcept.
//    tx = tx.send<Method>(...); // Block, throws.
//    future<T> ev = rx.recv<T>();    // Block, throws.
//    rx.recv<Method>(visitor_t());   // Nonblock and unpacked.
//
//    service.detach(); // Now dtor won't block.
//
//  Connection:
//    std::tie(tx, rx) = conn->invoke<I>().get();
//    std::tie(rx, res) = rx.recv<T>().get();
//    tx = tx.send<M>(); // May chain: tx.send<M1>().send<M2>();
//
//  Wrappers:
//    Primitive: conn->invoke<M>(args).get() -> value | error
//    Sequenced: conn->invoke<M>(args).get() -> rx<T, U, ...>
//    Streaming: conn->invoke<T>(args).get() -> (tx, rx).
//      rx.recv() -> T | E | C where T == dispatch_type, E - error type, C - choke.
//      rx.recv<T>() -> T | throw exception.
//      May throw error (network or protocol) or be exhaused (throw exception after E | C).

/// Test conn ctor.
/// Test conn connect.
/// Test conn connect failed.
/// Test conn async connect multiple times.
/// Test conn async connect multiple times when already connected.
// Test conn reconnect (recreate broken socket).
/// Test conn invoke.
// Test conn invoke multiple times - channel id must be increased.
/// Test conn invoke - network error - notify client.
// Test conn invoke - network error - notify all invokers.

// Test service ctor.
// Test service move ctor.
// Test service dtor (waits).
// Test service dtor after detach.
// Test service invoke - server received proper message.
// Test service invoke - server responds and the client receives and decodes proper message.
// Test service invoke - server responds and the client receives improper message.
// Test service invoke - server responds and the client receives an orphan message.
// Test service send.
// Test service send traverse.
// Test service send failed.
// Test service recv.
// Test service recv traverse.
// Test service recv failed.
// Test service connect.
// Test service connect failed.
// Test service async connect multiple times.
// Test service async connect multiple times when already connected.
// Test service reconnect on invalid connect.
// Test service timeout on connect.
// Test service timeout on invoke.
// Test service timeout on send(?).
// Test service timeout on recv.
// \note Strands will possibly be required.
// \note On worker side serialize all callbacks through a single thread (may be configured).
// Primitive protocol wrapper (value/error).
// \note Exception type guarantee.
// Service manager with thread pool (io loop pool, actually).
// GetService from SM.
// GetService async from SM.
// SM dtor.
// \note Internal thread safety.
// Test return version number expected (through T).
// Test error version mismatch.

// Test locator
// Test node
// Test storage
// Test echo.

// Test sync usage (background with thread).
// Test async usage (with single thread, but using nonblocking methods).
