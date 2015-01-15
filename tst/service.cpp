#include <future>
#include <thread>
#include <type_traits>

#include <boost/optional.hpp>

#include <gtest/gtest.h>

#include <cocaine/idl/node.hpp>

#include <cocaine/framework/service.hpp>

#include "mock.hpp"

using namespace cocaine::framework;

TEST(LowLevelService, Constructor) {
    loop_t loop;
    low_level_service<cocaine::io::mock> node("mock", loop);

    EXPECT_FALSE(node.connected());

    static_assert(
        std::is_nothrow_constructible<low_level_service<cocaine::io::mock>, std::string, loop_t&>::value,
        "service constructor must be noexcept"
    );
}

TEST(LowLevelService, Connect) {
    // ===== Set Up Stage =====
    // We create a TCP server in separate thread and wait for incoming connection. After accepting
    // just close the socket.
    // Note, that some rendezvous point is required to be sure, that the server has been started
    // when the client is trying to connect.
    loop_t server_loop;
    boost::asio::ip::tcp::acceptor acceptor(server_loop);

    // An OS should select available port for us.
    std::atomic<uint> port(0);
    boost::barrier barrier(2);
    std::packaged_task<void()> task([&server_loop, &acceptor, &barrier, &port]{
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

        barrier.wait();
        port.store(acceptor.local_endpoint().port());

        boost::asio::ip::tcp::socket socket(server_loop);
        acceptor.accept(socket);
    });

    std::future<void> server_future = task.get_future();
    std::thread server_thread(std::move(task));

    loop_t client_loop;
    std::thread client_thread([&client_loop]{
        loop_t::work work(client_loop);
        client_loop.run();
    });

    // Here we wait until the server has been started.
    barrier.wait();

    // ===== Test Stage =====
    low_level_service<cocaine::io::mock> service("mock", client_loop);

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
    boost::future<void> future = service.connect(endpoint);
    EXPECT_NO_THROW(future.get());

    EXPECT_TRUE(service.connected());

    // ===== Tear Down Stage =====
    acceptor.close();
    client_loop.stop();
    client_thread.join();

    server_thread.join();
    EXPECT_NO_THROW(server_future.get());
}

TEST(LowLevelService, ConnectOnInvalidPort) {
    // ===== Set Up Stage =====
    loop_t client_loop;
    std::thread client_thread([&client_loop]{
        loop_t::work work(client_loop);
        client_loop.run();
    });

    // ===== Test Stage =====
    low_level_service<cocaine::io::mock> service("mock", client_loop);

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 0);
    boost::future<void> future = service.connect(endpoint);
    EXPECT_THROW(future.get(), boost::system::system_error);

    EXPECT_FALSE(service.connected());

    // ===== Tear Down Stage =====
    client_loop.stop();
    client_thread.join();
}

TEST(LowLevelService, ConnectMultipleTimesOnDisconnectedService) {
    // ===== Set Up Stage =====
    loop_t server_loop;
    boost::asio::ip::tcp::acceptor acceptor(server_loop);

    std::atomic<uint> port(0);
    boost::barrier barrier(2);
    std::packaged_task<void()> task([&server_loop, &acceptor, &barrier, &port]{
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

        barrier.wait();
        port.store(acceptor.local_endpoint().port());

        boost::asio::ip::tcp::socket socket(server_loop);
        acceptor.accept(socket);
    });

    std::future<void> server_future = task.get_future();
    std::thread server_thread(std::move(task));

    loop_t client_loop;
    std::thread client_thread([&client_loop]{
        loop_t::work work(client_loop);
        client_loop.run();
    });

    barrier.wait();

    // ===== Test Stage =====
    low_level_service<cocaine::io::mock> service("mock", client_loop);

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
    auto f1 = service.connect(endpoint).then([&service](boost::future<void> f){
        EXPECT_NO_THROW(f.get());
        EXPECT_TRUE(service.connected());
    });

    auto f2 = service.connect(endpoint).then([&service](boost::future<void> f){
        EXPECT_NO_THROW(f.get());
        EXPECT_TRUE(service.connected());
    });

    boost::wait_for_all(f1, f2);

    // ===== Tear Down Stage =====
    acceptor.close();
    client_loop.stop();
    client_thread.join();

    server_thread.join();
    EXPECT_NO_THROW(server_future.get());
}

TEST(LowLevelService, ConnectOnConnectedService) {
    // ===== Set Up Stage =====
    loop_t server_loop;
    boost::asio::ip::tcp::acceptor acceptor(server_loop);

    std::atomic<uint> port(0);
    boost::barrier barrier(2);
    std::packaged_task<void()> task([&server_loop, &acceptor, &barrier, &port]{
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

        barrier.wait();
        port.store(acceptor.local_endpoint().port());

        boost::asio::ip::tcp::socket socket(server_loop);
        acceptor.accept(socket);
    });

    std::future<void> server_future = task.get_future();
    std::thread server_thread(std::move(task));

    loop_t client_loop;
    std::thread client_thread([&client_loop]{
        loop_t::work work(client_loop);
        client_loop.run();
    });

    barrier.wait();

    // ===== Test Stage =====
    low_level_service<cocaine::io::mock> service("mock", client_loop);

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
    service.connect(endpoint).get();
    EXPECT_TRUE(service.connected());

    EXPECT_NO_THROW(service.connect(endpoint).get());
    EXPECT_TRUE(service.connected());

    // ===== Tear Down Stage =====
    acceptor.close();
    client_loop.stop();
    client_thread.join();

    server_thread.join();
    EXPECT_NO_THROW(server_future.get());
}

TEST(LowLevelService, RAIIOnConnect) {
    // ===== Set Up Stage =====
    loop_t server_loop;
    boost::asio::ip::tcp::acceptor acceptor(server_loop);

    // An OS should select available port for us.
    std::atomic<uint> port(0);
    boost::barrier barrier(2);
    std::packaged_task<void()> task([&server_loop, &acceptor, &barrier, &port]{
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

        barrier.wait();
        port.store(acceptor.local_endpoint().port());

        boost::asio::ip::tcp::socket socket(server_loop);
        acceptor.accept(socket);
    });

    std::future<void> server_future = task.get_future();
    std::thread server_thread(std::move(task));

    loop_t client_loop;
    std::thread client_thread([&client_loop]{
        loop_t::work work(client_loop);
        client_loop.run();
    });

    // Here we wait until the server has been started.
    barrier.wait();

    // ===== Test Stage =====
    boost::optional<future_t<void>> future;
    {
        low_level_service<cocaine::io::mock> service("mock", client_loop);

        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        future = service.connect(endpoint);
    }
    EXPECT_NO_THROW(future->get());

    // ===== Tear Down Stage =====
    acceptor.close();
    client_loop.stop();
    client_thread.join();

    server_thread.join();
    EXPECT_NO_THROW(server_future.get());
}

// Usage:
    // auto chan = node.invoke<cocaine::io::node::list>(); // Nonblock, can throw.
    // auto tx = chan.tx.send<Method>(...);    // Block.
    // auto ev = chan.rx.recv<Method>();    // Block.
    // chan.rx.recv<Method>(visitor_t());   // Nonblock and unpacked.

    // service.detach(); // Now dtor won't block.

/// Test service ctor.
// Test service move ctor.
// Test service dtor (waits).
// Test service dtor after detach.
// Test invoke.
// Test send.
// Test send traverse.
// Test send failed.
// Test recv.
// Test recv traverse.
// Test recv failed.
/// Test connect.
/// Test connect failed.
/// Test async connect multiple times.
/// Test async connect multiple times on connected service.
// Test reconnect on invalid connect.
// Test timeout on connect.
// Test timeout on invoke.
// Test timeout on send(?).
// Test timeout on recv.
// Strands will possibly be required.
// On worker side serialize all callbacks through a single thread (may be configured).
// Primitive wrapper.
// Exception type guarantee.
// Service manager with thread pool (io loop pool, actually).
// GetService from SM.
// GetService async from SM.
// SM dtor.
// Internal thread safety(?).
// Test return version number expected (through T).
// Test error version mismatch.

// Test locator
// Test node
// Test storage
// Test echo.

// Test sync usage (background with thread).
// Test async usage (with single thread, but using nonblocking methods).
