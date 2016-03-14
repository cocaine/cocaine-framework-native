#include <gtest/gtest.h>

#include <cocaine/common.hpp>
#include <cocaine/idl/storage.hpp>
#include <cocaine/idl/streaming.hpp>
#include <cocaine/idl/node.hpp>
#include <cocaine/traits/error_code.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/scheduler.hpp>

#include "../../util/net.hpp"

using namespace cocaine::framework;

using namespace testing;
using namespace testing::util;

#include <cocaine/framework/manager.hpp>

TEST(service, NotFound) {
    service_manager_t manager(1);
    auto service = manager.create<cocaine::io::app_tag>("invalid");

    EXPECT_THROW(service.connect().get(), service_not_found);
}

TEST(service, ConnectionRefusedOnWrongLocator) {
    service_manager_t manager({{ boost::asio::ip::tcp::v6(), 10052 }}, 1);
    auto service = manager.create<cocaine::io::app_tag>("node");

    EXPECT_THROW(service.connect().get(), std::system_error);
}

namespace testing {

namespace mock {

struct storage_tag;

} // namespace mock

} // namespace testing

namespace cocaine { namespace io {

template<>
struct protocol<testing::mock::storage_tag> {
    typedef boost::mpl::int_<
        0
    >::type version;

    typedef boost::mpl::list<> messages;
};

}} // namespace cocaine::io

TEST(service, VersionMismatch) {
    service_manager_t manager(1);
    auto service = manager.create<mock::storage_tag>("storage");

    EXPECT_THROW(service.connect().get(), version_mismatch);
}

TEST(service, Storage) {
    service_manager_t manager(1);
    auto storage = manager.create<cocaine::io::storage_tag>("storage");
    auto result = storage.invoke<cocaine::io::storage::read>("collection", "key").get();

    EXPECT_EQ("le value", result);
}

TEST(service, StorageError) {
    service_manager_t manager(1);
    auto storage = manager.create<cocaine::io::storage_tag>("storage");

    EXPECT_THROW(storage.invoke<cocaine::io::storage::read>("i-collection", "key").get(), response_error);
}

TEST(service, Echo) {
    typedef cocaine::io::protocol<cocaine::io::app::enqueue::dispatch_type>::scope upstream;

    service_manager_t manager(1);
    auto echo = manager.create<cocaine::io::storage_tag>("echo-cpp");

    auto channel = echo.invoke<cocaine::io::app::enqueue>("ping").get();
    auto tx = std::move(channel.tx);
    auto rx = std::move(channel.rx);

    tx.send<upstream::chunk>("le message").get()
        .send<upstream::choke>().get();
    auto result = rx.recv().get();

    EXPECT_EQ("le message", *result);

    // Read the choke.
    rx.recv().get();
}

TEST(service, EchoHeaders) {
    typedef cocaine::io::protocol<cocaine::io::app::enqueue::dispatch_type>::scope upstream;

    service_manager_t manager(1);
    auto echo = manager.create<cocaine::io::storage_tag>("echo-cpp");

    auto channel = echo.invoke<cocaine::io::app::enqueue>("meta").get();
    auto tx = std::move(channel.tx);
    auto rx = std::move(channel.rx);

    tx.send<upstream::chunk>("le message").get()
        .send<upstream::choke>().get();
    auto result = rx.recv().get();

    EXPECT_EQ("le message", *result);

    // Read the choke.
    rx.recv().get();
}

namespace ph = std::placeholders;

namespace {

task<boost::optional<std::string>>::future_type
on_send(task<sender<cocaine::io::app::enqueue::dispatch_type, basic_session_t>>::future_move_type future,
        receiver<cocaine::io::app::enqueue::upstream_type, basic_session_t> rx)
{
    future.get();
    return rx.recv();
}

task<boost::optional<std::string>>::future_type
on_recv(task<boost::optional<std::string>>::future_move_type future,
        receiver<cocaine::io::app::enqueue::upstream_type, basic_session_t> rx)
{
    boost::optional<std::string> result = future.get();
    EXPECT_EQ("le message", *result);
    return rx.recv();
}

void
on_choke(task<boost::optional<std::string>>::future_move_type future) {
    auto result = future.get();
    EXPECT_FALSE(result);
}

task<void>::future_type
on_invoke(task<invocation_result<cocaine::io::app::enqueue>::type>::future_move_type future) {
    typedef cocaine::io::protocol<cocaine::io::app::enqueue::dispatch_type>::scope upstream;

    auto channel = future.get();
    auto tx = std::move(channel.tx);
    auto rx = std::move(channel.rx);
    return tx.send<upstream::chunk>("le message")
        .then(std::bind(&on_send, ph::_1, rx))
        .then(std::bind(&on_recv, ph::_1, rx))
        .then(std::bind(&on_choke, ph::_1));
}

} // namespace

TEST(service, EchoAsynchronous) {
    service_manager_t manager(1);
    auto echo = manager.create<cocaine::io::storage_tag>("echo-cpp");

    echo.invoke<cocaine::io::app::enqueue>("ping")
        .then(std::bind(&on_invoke, ph::_1))
        .get();
}
