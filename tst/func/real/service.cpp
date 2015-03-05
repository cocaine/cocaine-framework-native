#include <gtest/gtest.h>

#include <cocaine/idl/storage.hpp>
#include <cocaine/idl/streaming.hpp>
#include <cocaine/idl/node.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/scheduler.hpp>
#include <cocaine/framework/detail/loop.hpp>

#include "../../util/net.hpp"

using namespace cocaine::framework;

using namespace testing;
using namespace testing::util;

TEST(basic_service_t, Storage) {
    // Helper object, that runs event loop in the separate thread.
    client_t client;
    event_loop_t loop { client.loop() };
    scheduler_t scheduler(loop);

    // Service's constructor does nothing.
    basic_service_t storage("storage", 1, scheduler);

    // Invoke (and internally connect) read method.
    auto result = storage.invoke<cocaine::io::storage::read>(std::string("collection"), std::string("key")).get();

    EXPECT_EQ("le value", result);
}

TEST(basic_service_t, StorageError) {
    client_t client;
    event_loop_t loop { client.loop() };
    scheduler_t scheduler(loop);

    basic_service_t storage("storage", 1, scheduler);
    EXPECT_THROW(storage.invoke<cocaine::io::storage::read>(std::string("i-collection"), std::string("key")).get(), cocaine_error);
}

TEST(basic_service_t, Echo) {
    typedef typename cocaine::io::protocol<cocaine::io::app::enqueue::dispatch_type>::scope upstream;

    client_t client;
    event_loop_t loop { client.loop() };
    scheduler_t scheduler(loop);

    basic_service_t echo("echo", 1, scheduler);
    auto ch = echo.invoke<cocaine::io::app::enqueue>(std::string("ping")).get();
    auto tx = std::move(std::get<0>(ch));
    auto rx = std::move(std::get<1>(ch));
    tx.send<upstream::chunk>(std::string("le message")).get();
    auto result = rx.recv().get();
    rx.recv().get();

    EXPECT_EQ("le message", *result);
}

TEST(basic_service_t, EchoMT) {
    typedef typename cocaine::io::protocol<cocaine::io::app::enqueue::dispatch_type>::scope upstream;

    client_t client;
    event_loop_t loop { client.loop() };
    scheduler_t scheduler(loop);
    std::vector<boost::thread> threads;

    for (size_t tid = 0; tid < 8; ++tid) {
        threads.emplace_back(boost::thread([tid, &scheduler]{
            basic_service_t echo("echo-cpp", 1, scheduler);

            for (size_t id = 0; id < 10; ++id) {
                auto ch = echo.invoke<cocaine::io::app::enqueue>(std::string("ping")).get();
                auto tx = std::move(std::get<0>(ch));
                auto rx = std::move(std::get<1>(ch));
                auto chunk = std::to_string(tid) + "/" + std::to_string(id) + ": le message";
                tx.send<upstream::chunk>(chunk).get();
                auto result = rx.recv().get();
                rx.recv().get();

                EXPECT_EQ(chunk, *result);
                if (chunk != *result) {
                    std::terminate();
                }
            }
        }));
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

#include <cocaine/framework/manager.hpp>

TEST(service, NotFound) {
    service_manager_t manager;
    auto service = manager.create<cocaine::io::app_tag>("invalid");

    EXPECT_THROW(service->connect().get(), service_not_found_error);
}

TEST(service, Storage) {
    service_manager_t manager;
    auto storage = manager.create<cocaine::io::storage_tag>("storage");
    auto result = storage->invoke<cocaine::io::storage::read>(std::string("collection"), std::string("key")).get();

    EXPECT_EQ("le value", result);
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
on_choke(typename task<boost::optional<std::string>>::future_move_type future)
{
    auto result = future.get();
    EXPECT_FALSE(result);
}

typename task<void>::future_type
on_invoke(typename task<typename invocation_result<cocaine::io::app::enqueue>::type>::future_move_type future,
          scheduler_t& scheduler) {
    typedef typename cocaine::io::protocol<cocaine::io::app::enqueue::dispatch_type>::scope upstream;

    auto ch = future.get();
    auto tx = std::move(std::get<0>(ch));
    auto rx = std::move(std::get<1>(ch));
    return tx.send<upstream::chunk>(std::string("le message"))
        .then(scheduler, std::bind(&on_send, ph::_1, rx))
        .then(scheduler, std::bind(&on_recv, ph::_1, rx))
        .then(scheduler, std::bind(&on_choke, ph::_1));
}

} // namespace

TEST(service, EchoAsynchronous) {
    client_t client;
    event_loop_t loop { client.loop() };
    scheduler_t scheduler(loop);
    basic_service_t echo("echo-cpp", 1, scheduler);

    echo.invoke<cocaine::io::app::enqueue>(std::string("ping"))
        .then(scheduler, std::bind(&on_invoke, ph::_1, std::ref(scheduler)))
        .get();
}

TEST(service, EchoAsynchronousMultiple) {
    client_t client;
    event_loop_t loop { client.loop() };
    scheduler_t scheduler(loop);
    basic_service_t echo("echo-cpp", 1, scheduler);

    std::vector<typename task<void>::future_type> futures;
    for (int i = 0; i < 1000; ++i) {
        futures.emplace_back(
            echo.invoke<cocaine::io::app::enqueue>(std::string("ping"))
                .then(scheduler, std::bind(&on_invoke, ph::_1, std::ref(scheduler)))
        );
    }

    // Block here.
    for (auto& future : futures) {
        future.get();
    }
}
