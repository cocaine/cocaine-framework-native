#include <gtest/gtest.h>

#include <cocaine/idl/storage.hpp>
#include <cocaine/idl/streaming.hpp>
#include <cocaine/idl/node.hpp>

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

    EXPECT_THROW(service.connect().get(), service_not_found_error);
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

    EXPECT_THROW(service.connect().get(), version_mismatch_error);
}

TEST(service, Storage) {
    service_manager_t manager(1);
    auto storage = manager.create<cocaine::io::storage_tag>("storage");
    auto result = storage.invoke<cocaine::io::storage::read>(std::string("collection"), std::string("key")).get();

    EXPECT_EQ("le value", result);
}

TEST(service, StorageError) {
    service_manager_t manager(1);
    auto storage = manager.create<cocaine::io::storage_tag>("storage");

    EXPECT_THROW(storage.invoke<cocaine::io::storage::read>(std::string("i-collection"), std::string("key")).get(), error_t);
}

TEST(service, Echo) {
    typedef typename cocaine::io::protocol<cocaine::io::app::enqueue::dispatch_type>::scope upstream;

    service_manager_t manager(1);
    auto echo = manager.create<cocaine::io::storage_tag>("echo-cpp");

    auto channel = echo.invoke<cocaine::io::app::enqueue>(std::string("ping")).get();
    auto tx = std::move(std::get<0>(channel));
    auto rx = std::move(std::get<1>(channel));

    tx.send<upstream::chunk>(std::string("le message")).get();
    auto result = rx.recv().get();

    EXPECT_EQ("le message", *result);
}

namespace ph = std::placeholders;

namespace {

typename task<boost::optional<std::string>>::future_type
on_send(typename task<sender<cocaine::io::app::enqueue::dispatch_type, basic_session_t>>::future_move_type future,
        receiver<cocaine::io::app::enqueue::upstream_type, basic_session_t> rx)
{
    future.get();
    return rx.recv();
}

typename task<boost::optional<std::string>>::future_type
on_recv(typename task<boost::optional<std::string>>::future_move_type future,
        receiver<cocaine::io::app::enqueue::upstream_type, basic_session_t> rx)
{
    boost::optional<std::string> result = future.get();
    EXPECT_EQ("le message", *result);
    return rx.recv();
}

void
on_choke(typename task<boost::optional<std::string>>::future_move_type future) {
    auto result = future.get();
    EXPECT_FALSE(result);
}

typename task<void>::future_type
on_invoke(typename task<typename invocation_result<cocaine::io::app::enqueue>::type>::future_move_type future) {
    typedef typename cocaine::io::protocol<cocaine::io::app::enqueue::dispatch_type>::scope upstream;

    auto ch = future.get();
    auto tx = std::move(std::get<0>(ch));
    auto rx = std::move(std::get<1>(ch));
    return tx.send<upstream::chunk>(std::string("le message"))
        .then(std::bind(&on_send, ph::_1, rx))
        .then(std::bind(&on_recv, ph::_1, rx))
        .then(std::bind(&on_choke, ph::_1));
}

} // namespace

TEST(service, EchoAsynchronous) {
    service_manager_t manager(1);
    auto echo = manager.create<cocaine::io::storage_tag>("echo-cpp");

    echo.invoke<cocaine::io::app::enqueue>(std::string("ping"))
        .then(std::bind(&on_invoke, ph::_1))
        .get();
}
