#include <type_traits>

#include <boost/thread/barrier.hpp>
#include <boost/thread/thread.hpp>

#include <asio/read.hpp>
#include <asio/write.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cocaine/idl/locator.hpp>
#include <cocaine/idl/node.hpp>
#include <cocaine/idl/streaming.hpp>

#include <cocaine/common.hpp>
#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/graph.hpp>
#include <cocaine/traits/vector.hpp>

#include <cocaine/framework/session.hpp>

#include "mock/event.hpp"
#include "util/net.hpp"

using namespace cocaine::framework;

using namespace testing;
using namespace testing::util;


TEST(basic_session_t, Constructor) {
    loop_t loop;
    auto session = std::make_shared<basic_session_t>(loop);

    EXPECT_FALSE(session->connected());

    static_assert(std::is_nothrow_constructible<basic_session_t, loop_t&>::value, "must be noexcept");
}

TEST(basic_session_t, Connect) {
    // ===== Set Up Stage =====
    // We create a TCP server in separate thread and wait for incoming connection. After accepting
    // just close the socket.
    // Note, that some rendezvous point is required to be sure, that the server has been started
    // when the client is trying to connect.

    // An OS should select available port for us.
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [](const std::error_code& ec){
            EXPECT_EQ(0, ec.value());
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    auto future = session->connect(endpoint);
    EXPECT_EQ(std::error_code(), future.get());
    EXPECT_TRUE(session->connected());

    server.stop();
}

TEST(basic_session_t, ConnectOnInvalidPort) {
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), 1);

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    auto future = session->connect(endpoint);
    EXPECT_EQ(io::error::connection_refused, future.get());
    EXPECT_FALSE(session->connected());
}

TEST(basic_session_t, ConnectMultipleTimesOnDisconnectedService) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [](const std::error_code& ec){
            EXPECT_EQ(0, ec.value());
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    auto f1 = session->connect(endpoint).then([&session](task<std::error_code>::future_move_type f){
        EXPECT_EQ(std::error_code(), f.get());
        EXPECT_TRUE(session->connected());
    });

    auto f2 = session->connect(endpoint).then([&session](task<std::error_code>::future_move_type f){
        EXPECT_THAT(f.get(), AnyOf(io::error::already_started, io::error::already_connected));
    });

    f1.get();
    f2.get();

    server.stop();
}

TEST(basic_session_t, ConnectOnConnectedService) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    //! \note the server should run in its main function scope until the end of the test.
    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [](const std::error_code& ec){
            EXPECT_EQ(0, ec.value());
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();

    auto future = session->connect(endpoint);
    EXPECT_EQ(io::error::already_connected, future.get());
    EXPECT_TRUE(session->connected());

    server.stop();
}

//! \brief Test, that the connect future is valid even after session destruction.
TEST(basic_session_t, RAIIOnConnect) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [](const std::error_code& ec){
            EXPECT_EQ(0, ec.value());
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    task<std::error_code>::future_type future;
    {
        auto session = std::make_shared<basic_session_t>(client.loop());
        future = std::move(session->connect(endpoint));
    }
    EXPECT_EQ(std::error_code(), future.get());

    server.stop();
}

TEST(Encoder, InvokeEvent) {
    cocaine::io::encoded<cocaine::io::locator::resolve> message(1, std::string("node"));

    const std::vector<std::uint8_t> expected = {{ 147, 1, 0, 145, 164, 110, 111, 100, 101 }};

    EXPECT_EQ(expected, std::vector<std::uint8_t>(message.data(), message.data() + 9));
}

TEST(basic_session_t, InvokeSendsProperMessage) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        std::array<std::uint8_t, 9> actual;
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&socket, &actual](const std::error_code&){
            io::async_read(socket, io::buffer(actual), [&actual](const std::error_code& ec, size_t size){
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

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();
    auto f = session->invoke<cocaine::io::locator::resolve>(std::string("node"));

    //! \note wait to prevent RC.
    f.get();

    //! \note stop the server to be sure, that the client receives EOF.
    server.stop();
}

//! This test checks, that channel id is increasing.
TEST(basic_session_t, InvokeMultipleTimesSendsProperMessages) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        std::array<std::uint8_t, 18> actual;
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&socket, &actual](const std::error_code&){
            io::async_read(socket, io::buffer(actual), [&actual](const std::error_code& ec, size_t size){
                EXPECT_EQ(18, size);
                EXPECT_EQ(0, ec.value());

                std::array<std::uint8_t, 9> expected1 = {{ 147, 1, 0, 145, 164, 110, 111, 100, 101 }};
                std::array<std::uint8_t, 9> expected2 = {{ 147, 2, 0, 145, 164, 101,  99, 104, 111 }};
                for (int i = 0; i < 9; ++i) {
                    EXPECT_EQ(expected1[i], actual[i]);
                }
                for (int i = 0; i < 9; ++i) {
                    EXPECT_EQ(expected2[i], actual[9 + i]);
                }
            });
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();
    session->invoke<cocaine::io::locator::resolve>(std::string("node"));
    session->invoke<cocaine::io::locator::resolve>(std::string("echo"));

    server.stop();
}

TEST(basic_session_t, InvokeWhileServerClosesConnection) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    boost::barrier barrier(2);
    server_t server(port, [&barrier](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&socket, &barrier](const std::error_code&){
            socket.shutdown(asio::ip::tcp::socket::shutdown_both);
            socket.close();

            barrier.wait();
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();

    // Wait until the server closes the connection.
    barrier.wait();

    session->invoke<cocaine::io::locator::resolve>(std::string("node"));

    server.stop();
}

TEST(basic_session_t, DecodeIncomingMessage) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        std::array<std::uint8_t, 4> incoming;

        // The following sequence is an encoded [1, 0, [['echo', 'http']]] struct, which is the
        // real response from Node service's 'list' request.
        // The caller service must properly unpack it and return to the user.
        const std::array<std::uint8_t, 15> buf = {
            { 147, 1, 0, 145, 146, 164, 101, 99, 104, 111, 164, 104, 116, 116, 112 }
        };
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&socket, &buf, &incoming](const std::error_code&){
            //! \note sometimes a race condition can occur - the socket should be read first.
            io::async_read(socket, io::buffer(incoming), [&socket, &buf](const std::error_code& ec, size_t){
                EXPECT_EQ(0, ec.value());

                io::async_write(socket, io::buffer(buf), [](const std::error_code& ec, size_t size){
                    EXPECT_EQ(0, ec.value());
                    EXPECT_EQ(15, size);
                });
            });
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();

    auto chan = session->invoke<mock::event::list>().get();
    auto rx = std::get<1>(chan);
    auto result = rx->recv().get();
    auto apps = boost::get<std::vector<std::string>>(result);
    EXPECT_EQ(std::vector<std::string>({ "echo", "http" }), apps);

    server.stop();
}

TEST(basic_session_t, InvokeAndReceiveWhileServerClosesConnection) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    boost::barrier barrier(2);
    server_t server(port, [&barrier](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&socket, &barrier](const std::error_code&){
            socket.shutdown(asio::ip::tcp::socket::shutdown_both);
            socket.close();

            barrier.wait();
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();

    // Wait until the server closes the connection.
    barrier.wait();

    // Depending on reader state (it can either receive EOF or not yet) the following method
    // possibly can throw a system exception with ENOTCONN errno.
    try {
        auto ch = session->invoke<cocaine::io::locator::resolve>(std::string("node")).get();
        auto rx = std::get<1>(ch);
        EXPECT_THROW(rx->recv().get(), std::system_error);
    } catch (const std::system_error&) {
    }

    server.stop();
}

TEST(basic_session_t, InvokeAndReceiveWhileConnectionResetByPeer) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    boost::barrier barrier(2);
    server_t server(port, [&barrier](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&socket, &barrier](const std::error_code&){
            socket.set_option(io::socket_base::linger(true, 0));
            socket.close();

            barrier.wait();
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();

    // Wait until the server closes the connection.
    barrier.wait();

    // Depending on reader state (it can either receive EOF or not yet) the following method
    // possibly can throw a system exception with ENOTCONN errno.
    try {
        auto ch = session->invoke<cocaine::io::locator::resolve>(std::string("node")).get();
        auto rx = std::get<1>(ch);
        EXPECT_THROW(rx->recv().get(), std::system_error);
    } catch (const std::system_error&) {
    }

    server.stop();
}

TEST(basic_session_t, InvokeMultipleTimesWhileServerClosesConnection) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        std::array<std::uint8_t, 32> actual;
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&socket, &actual](const std::error_code&){
            socket.shutdown(asio::ip::tcp::socket::shutdown_both);
            socket.close();
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();

    try {
        auto ch1 = session->invoke<cocaine::io::locator::resolve>(std::string("node")).get();
        auto ch2 = session->invoke<cocaine::io::locator::resolve>(std::string("node")).get();
        auto rx1 = std::get<1>(ch1);
        auto rx2 = std::get<1>(ch2);
        EXPECT_THROW(rx1->recv().get(), std::system_error);
        EXPECT_THROW(rx2->recv().get(), std::system_error);
    } catch (const std::system_error&) {
    }

    server.stop();
}

TEST(basic_session_t, SendSendsProperMessage) {
    typedef cocaine::io::protocol<
        cocaine::io::event_traits<
            cocaine::io::app::enqueue
        >::dispatch_type
    >::scope protocol;

    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        std::array<std::uint8_t, 24> actual;
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&socket, &actual](const std::error_code&){
            io::async_read(socket, io::buffer(actual), [&actual](const std::error_code& ec, size_t size){
                EXPECT_EQ(24, size);
                EXPECT_EQ(0, ec.value());

                std::array<std::uint8_t, 9> invoke = {{ 147, 1, 0, 145, 164, 112, 105, 110, 103 }};
                std::array<std::uint8_t, 15> send =  {{ 147, 1, 0, 145, 170, 108, 101,  32, 109, 101, 115, 115, 97, 103, 101 }};
                for (int i = 0; i < 9; ++i) {
                    EXPECT_EQ(invoke[i], actual[i]);
                }

                for (int i = 0; i < 9; ++i) {
                    EXPECT_EQ(send[i], actual[9 + i]);
                }
            });
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();

    auto ch = session->invoke<cocaine::io::app::enqueue>(std::string("ping")).get();
    auto tx = std::get<0>(ch);

    tx->send<protocol::chunk>(std::string("le message")).get();

    server.stop();
}

TEST(basic_session_t, SendOnClosedSocket) {
    typedef cocaine::io::protocol<
        cocaine::io::event_traits<
            cocaine::io::app::enqueue
        >::dispatch_type
    >::scope protocol;

    // ===== Set Up Stage =====
    boost::barrier barrier(2);
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [&barrier](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        std::array<std::uint8_t, 9> actual;
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&socket, &actual, &barrier](const std::error_code&){
            io::async_read(socket, io::buffer(actual), [&socket, &actual, &barrier](const std::error_code& ec, size_t size){
                EXPECT_EQ(9, size);
                EXPECT_EQ(0, ec.value());

                std::array<std::uint8_t, 9> invoke = {{ 147, 1, 0, 145, 164, 112, 105, 110, 103 }};
                for (int i = 0; i < 9; ++i) {
                    EXPECT_EQ(invoke[i], actual[i]);
                }

                socket.shutdown(asio::ip::tcp::socket::shutdown_both);

                barrier.wait();
            });
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();
    auto ch = session->invoke<cocaine::io::app::enqueue>(std::string("ping")).get();

    barrier.wait();

    try {
        auto tx = std::get<0>(ch);
        tx->send<protocol::chunk>(std::string("le message")).get();

        auto rx = std::get<1>(ch);
        EXPECT_THROW(rx->recv().get(), std::system_error);
    } catch (const std::runtime_error&) {
    }

    server.stop();
}

TEST(basic_session_t, SilentlyDropOrphanMessageButContinueToListen) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        std::array<std::uint8_t, 4> incoming;
        std::array<io::const_buffer, 2> bufs = {
            io::buffer(std::array<std::uint8_t, 15> {
                { 147, 2, 0, 145, 146, 164, 101, 99, 104, 111, 164, 104, 116, 116, 112 }
            }),
            io::buffer(std::array<std::uint8_t, 15> {
                { 147, 1, 0, 145, 146, 164, 101, 99, 104, 111, 164, 104, 116, 116, 112 }
            })
        };
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&socket, &incoming, &bufs](const std::error_code&){
            //! \note sometimes a race condition can occur - the socket should be read first.
            io::async_read(socket, io::buffer(incoming), [&socket, &bufs](const std::error_code& ec, size_t){
                EXPECT_EQ(0, ec.value());

                io::async_write(socket, bufs, [](const std::error_code& ec, size_t size){
                    EXPECT_EQ(0, ec.value());
                    EXPECT_EQ(30, size);
                });
            });
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();

    auto ch = session->invoke<mock::event::list>().get();

    auto rx = std::get<1>(ch);
    auto result = rx->recv().get();
    auto list = boost::get<std::vector<std::string>>(result);
    EXPECT_EQ(std::vector<std::string>({ "echo", "http" }), list);

    server.stop();
}

//! The test should result in ECANCELED connection error in the connection future.
TEST(basic_session_t, ManualDisconnectWhileConnecting) {
    //! \todo I couldn't write proper code to check this behavior :(
}

TEST(basic_session_t, ManualDisconnectWhileInvoke) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    boost::barrier barrier(2);
    server_t server(port, [&barrier](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&socket, &barrier](const std::error_code& ec){
            EXPECT_EQ(0, ec.value());
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();

    auto future = session->invoke<cocaine::io::locator::resolve>(std::string("node"));

    session->disconnect(io::error::operation_aborted);

    try {
        // \note either the invocation future or the receiver can throw an error.
        auto ch = future.get();
        auto rx = std::get<1>(ch);
        try {
            rx->recv().get();
        } catch (const std::system_error& ec) {
            EXPECT_EQ(io::error::operation_aborted, ec.code());
        }
    } catch (const std::system_error& ec) {
        EXPECT_EQ(io::error::operation_aborted, ec.code());
    }

    server.stop();
}

TEST(basic_session_t, ManualDisconnectWhileRecv) {
    // ===== Set Up Stage =====
    const std::uint16_t port = testing::util::port();
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

    boost::barrier barrier(2);
    server_t server(port, [&barrier](io::ip::tcp::acceptor& acceptor, loop_t& loop){
        io::ip::tcp::socket socket(loop);
        acceptor.async_accept(socket, [&socket, &barrier](const std::error_code& ec){
            EXPECT_EQ(0, ec.value());
        });

        EXPECT_NO_THROW(loop.run());
    });

    client_t client;

    // ===== Test Stage =====
    auto session = std::make_shared<basic_session_t>(client.loop());
    session->connect(endpoint).get();

    auto ch = session->invoke<cocaine::io::locator::resolve>(std::string("node")).get();
    auto rx = std::get<1>(ch);
    session->disconnect(io::error::operation_aborted);

    try {
        rx->recv().get();
    } catch (const std::system_error& ec) {
        EXPECT_EQ(io::error::operation_aborted, ec.code());
    }

    server.stop();
}

// Usage:
//  basic_session_t - The base class, does almost all the job:
//    std::tie(tx, rx) = session->invoke<E>();
//    std::tie(rx, res) = rx.recv();
//    tx = tx.send<C>(); // May chain: tx.send<C1>().send<C2>();
//
//  Service - Proxy with auto resolving and RAII style:
//    std::tie(tx, rx) = node.invoke<E>(); // Nonblock.
//    tx = tx.send<C>(...);                // Nonblock, returns the following Sender<...>. May throw.
//    std::tie(rx, res) = rx.recv();       // Returns tuple with the following Receiver<...> and Future<V>. No throw.
//
//    service.detach(); // Now the service's destrucor won't block.
//
//  Wrappers:
//    Primitive + void Dispatch [almost all services]:
//      session->invoke<M>(args).get() -> value | error
//    Sequenced + void Dispatch [not implemented]:
//      session->invoke<M>(args).get() -> Receiver<T, U, ...>
//    Streaming: session->invoke<M>(args).get() -> (tx, rx).
//      rx.recv() -> T | E | C where T == dispatch_type, E - error type, C - choke.
//      rx.recv<T>() -> optional<T> | throw exception.
//      May throw error (network or protocol) or be exhaused (throw exception after E | C).

/// Test session ctor.
/// Test session connect.
/// Test session connect failed.
/// Test session async connect multiple times.
/// Test session async connect multiple times when already connected.
/// Test session invoke.
/// Test session invoke multiple times - channel id must be increased.
/// Test session invoke - network error on write - notify client.
/// Test session invoke and recv - ok.
// Test session invoke and recv invalid message, then valid.
/// Test session invoke - network error on read - notify client.
/// Test session invoke - network error on read - notify all invokers.
/// Test session invoke and send - ok.
/// Test session invoke and send - error - notify client.
// Test session invoke and send multiple times - error - notify only once.
// Test session connect and disconnect while connecting.
/// Test session invoke and disconnect while it invoking (or after).
/// Test session invoke and disconnect while it receiving bytes.

// Do not insert channels for mute events.
