#include <asio/read.hpp>
#include <asio/write.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

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
    typedef cocaine::framework::basic_session_t::endpoint_type endpoint_type;

    basic_session_t(scheduler_t&) {}

    MOCK_CONST_METHOD0(connected, bool());
    MOCK_METHOD1(connect, task<std::error_code>::future_move_type(const std::vector<endpoint_type>&));

    task<std::error_code>::future_move_type
    connect(const endpoint_type& endpoint) {
        return connect(std::vector<endpoint_type> {{ endpoint }});
    }

    void disconnect() {}
    bool connected() { return false; }
};

} // namespace mock

} // namespace testing

#include "src/session.cpp"

namespace cocaine {
namespace framework {

template class session<testing::mock::basic_session_t>;

}
}

TEST(session, Constructor) {
    auto d = std::make_shared<mock::basic_session_t>();
    auto s = std::make_shared<session<mock::basic_session_t>>(d);

    EXPECT_CALL(*d, connected())
        .WillOnce(Return(false));

    EXPECT_FALSE(s->connected());
}

TEST(session, Connect) {
    const basic_session_t::endpoint_type endpoint(boost::asio::ip::tcp::v4(), 42);

    auto d = std::make_shared<mock::basic_session_t>();
    auto s = std::make_shared<session<mock::basic_session_t>>(d);

    auto f = make_ready_future<std::error_code>::value(std::error_code());

    EXPECT_CALL(*d, connect(std::vector<basic_session_t::endpoint_type> {{ endpoint }}))
        .WillOnce(ReturnRef(f));

    EXPECT_NO_THROW(s->connect(endpoint).get());
}

TEST(session, ConnectionRefused) {
    const basic_session_t::endpoint_type endpoint(boost::asio::ip::tcp::v4(), 42);

    auto d = std::make_shared<mock::basic_session_t>();
    auto s = std::make_shared<session<mock::basic_session_t>>(d);

    auto f = make_ready_future<std::error_code>::value(asio::error::connection_refused);

    EXPECT_CALL(*d, connect(std::vector<basic_session_t::endpoint_type> {{ endpoint }}))
        .WillOnce(ReturnRef(f));

    EXPECT_THROW(s->connect(endpoint).get(), std::system_error);
}

TEST(session, ConnectWhileConnecting) {
    const basic_session_t::endpoint_type endpoint(boost::asio::ip::tcp::v4(), 42);

    auto d = std::make_shared<mock::basic_session_t>();
    auto s = std::make_shared<session<mock::basic_session_t>>(d);

    typename task<std::error_code>::promise_type p1;
    auto f1 = p1.get_future();
    auto f2 = make_ready_future<std::error_code>::value(asio::error::already_started);

    EXPECT_CALL(*d, connect(std::vector<basic_session_t::endpoint_type> {{ endpoint }}))
        .WillOnce(ReturnRef(f1))
        .WillOnce(ReturnRef(f2));

    auto f11 = s->connect(endpoint);
    auto f12 = s->connect(endpoint);

    p1.set_value(std::error_code());

    EXPECT_NO_THROW(f11.get());
    EXPECT_NO_THROW(f12.get());
}

TEST(session, ConnectWhileConnectingError) {
    const basic_session_t::endpoint_type endpoint(boost::asio::ip::tcp::v4(), 42);

    auto d = std::make_shared<mock::basic_session_t>();
    auto s = std::make_shared<session<mock::basic_session_t>>(d);

    typename task<std::error_code>::promise_type p1;
    auto f1 = p1.get_future();
    auto f2 = make_ready_future<std::error_code>::value(asio::error::already_started);

    EXPECT_CALL(*d, connect(std::vector<basic_session_t::endpoint_type> {{ endpoint }}))
        .WillOnce(ReturnRef(f1))
        .WillOnce(ReturnRef(f2));

    auto f11 = s->connect(endpoint);
    auto f12 = s->connect(endpoint);

    p1.set_value(asio::error::connection_refused);

    EXPECT_THROW(f11.get(), std::system_error);
    EXPECT_THROW(f12.get(), std::system_error);
}

/// functional
// - connect 1 ok.
// - connect 1 err - throw.
// - connect 2 seq ok.
// - connect 2 1by1 ok.
// - connect 2 seq with error
// - connect 2 different endpoint seq - throw
// - connect different endpoint when connected
// = 70m + refactoring 20m => 1.5h
