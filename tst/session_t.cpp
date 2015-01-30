#include <asio/read.hpp>
#include <asio/write.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cocaine/framework/connection.hpp>
#include <cocaine/framework/session.hpp>

#include "mock/event.hpp"
#include "util/net.hpp"

using namespace cocaine::framework;

using namespace testing;
using namespace testing::util;

namespace testing {

namespace mock {

class basic_session_t {
public:
    MOCK_CONST_METHOD0(connected, bool());
    MOCK_METHOD1(connect, future_t<std::error_code>&(const endpoint_t&));
};

} // namespace mock

} // namespace testing

TEST(session, Constructor) {
    auto d = std::make_shared<mock::basic_session_t>();
    auto s = std::make_shared<session<mock::event_tag, mock::basic_session_t>>(d);

    EXPECT_CALL(*d, connected())
        .WillOnce(Return(false));

    EXPECT_FALSE(s->connected());
}

TEST(session, Connect) {
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), 42);

    auto d = std::make_shared<mock::basic_session_t>();
    auto s = std::make_shared<session<mock::event_tag, mock::basic_session_t>>(d);

    auto f = make_ready_future<std::error_code>::value(std::error_code());

    EXPECT_CALL(*d, connect(endpoint))
        .WillOnce(ReturnRef(f));

    EXPECT_NO_THROW(s->connect(endpoint).get());
}

TEST(session, ConnectionRefused) {
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), 42);

    auto d = std::make_shared<mock::basic_session_t>();
    auto s = std::make_shared<session<mock::event_tag, mock::basic_session_t>>(d);

    auto f = make_ready_future<std::error_code>::value(io::error::connection_refused);

    EXPECT_CALL(*d, connect(endpoint))
        .WillOnce(ReturnRef(f));

    EXPECT_THROW(s->connect(endpoint).get(), std::system_error);
}

TEST(session, ConnectWhileConnecting) {
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), 42);

    auto d = std::make_shared<mock::basic_session_t>();
    auto s = std::make_shared<session<mock::event_tag, mock::basic_session_t>>(d);

    promise_t<std::error_code> p1;
    auto f1 = p1.get_future();
    auto f2 = make_ready_future<std::error_code>::value(io::error::already_started);

    EXPECT_CALL(*d, connect(endpoint))
        .WillOnce(ReturnRef(f1))
        .WillOnce(ReturnRef(f2));

    auto f11 = s->connect(endpoint);
    auto f12 = s->connect(endpoint);

    p1.set_value(std::error_code());

    EXPECT_NO_THROW(f11.get());
    EXPECT_NO_THROW(f12.get());
}

TEST(session, ConnectWhileConnectingError) {
    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), 42);

    auto d = std::make_shared<mock::basic_session_t>();
    auto s = std::make_shared<session<mock::event_tag, mock::basic_session_t>>(d);

    promise_t<std::error_code> p1;
    auto f1 = p1.get_future();
    auto f2 = make_ready_future<std::error_code>::value(io::error::already_started);

    EXPECT_CALL(*d, connect(endpoint))
        .WillOnce(ReturnRef(f1))
        .WillOnce(ReturnRef(f2));

    auto f11 = s->connect(endpoint);
    auto f12 = s->connect(endpoint);

    p1.set_value(io::error::connection_refused);

    EXPECT_THROW(f11.get(), std::system_error);
    EXPECT_THROW(f12.get(), std::system_error);
}

// TODO: Connect different endpoints.

//TEST(session, __InvokeSendsProperMessage) {
//    // ===== Set Up Stage =====
//    const std::uint16_t port = testing::util::port();
//    const io::ip::tcp::endpoint endpoint(io::ip::tcp::v4(), port);

//    server_t server(port, [](io::ip::tcp::acceptor& acceptor, loop_t& loop){
//        std::array<std::uint8_t, 9> actual;
//        io::ip::tcp::socket socket(loop);
//        acceptor.async_accept(socket, [&socket, &actual](const std::error_code&){
//            io::async_read(socket, io::buffer(actual), [&actual](const std::error_code& ec, size_t size){
//                EXPECT_EQ(9, size);
//                EXPECT_EQ(0, ec.value());

//                std::array<std::uint8_t, 9> expected = {{ 147, 1, 0, 145, 164, 110, 111, 100, 101 }};
//                for (int i = 0; i < 9; ++i) {
//                    EXPECT_EQ(expected[i], actual[i]);
//                }
//            });
//        });

//        EXPECT_NO_THROW(loop.run());
//    });

//    client_t client;

//    // ===== Test Stage =====
//    auto bs = std::make_shared<basic_session_t>(client.loop());
//    auto s = std::make_shared<session<mock::event_tag>>(bs);
//    s->connect(endpoint).get();
//    auto f = s->invoke<mock::event::list>();

//    //! \note wait to prevent RC.
//    f.get();

//    //! \note stop the server to be sure, that the client receives EOF.
//    server.stop();
//}
