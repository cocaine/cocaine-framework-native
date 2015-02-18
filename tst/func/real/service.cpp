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

TEST(service, Storage) {
    // Helper object, that runs event loop in the separate thread.
    client_t client;
    event_loop_t loop { client.loop() };
    scheduler_t scheduler(loop);

    // Service's constructor does nothing.
    service<cocaine::io::storage_tag> storage("storage", scheduler);

    // Invoke (and internally connect) read method.
    auto result = storage.invoke<cocaine::io::storage::read>(std::string("collection"), std::string("key")).get();

    EXPECT_EQ("le value", result);
}

TEST(service, StorageError) {
    client_t client;
    event_loop_t loop { client.loop() };
    scheduler_t scheduler(loop);

    service<cocaine::io::storage_tag> storage("storage", scheduler);
    EXPECT_THROW(storage.invoke<cocaine::io::storage::read>(std::string("i-collection"), std::string("key")).get(), cocaine_error);
}

TEST(service, Echo) {
    typedef typename cocaine::io::protocol<cocaine::io::app::enqueue::dispatch_type>::scope upstream;

    client_t client;
    event_loop_t loop { client.loop() };
    scheduler_t scheduler(loop);

    service<cocaine::io::app_tag> echo("echo", scheduler);
    auto ch = echo.invoke<cocaine::io::app::enqueue>(std::string("ping")).get();
    auto tx = std::move(std::get<0>(ch));
    auto rx = std::move(std::get<1>(ch));
    tx.send<upstream::chunk>(std::string("le message")).get();
    auto result = rx.recv().get();
    rx.recv().get();

    EXPECT_EQ("le message", *result);
}

//TEST(service, EchoMT) {
//    typedef typename cocaine::io::protocol<cocaine::io::app::enqueue::dispatch_type>::scope upstream;

//    std::vector<boost::thread> threads;
//    for (size_t tid = 0; tid < 8; ++tid) {
//        client_t client;
//        service<cocaine::io::app_tag> echo("echo-cpp", client.loop());

//        threads.push_back(boost::thread([tid, &echo]{
//            for (size_t id = 0; id < 250; ++id) {
//                auto ch = echo.invoke<cocaine::io::app::enqueue>(std::string("ping")).get();
//                auto tx = std::move(std::get<0>(ch));
//                auto rx = std::move(std::get<1>(ch));
//                auto chunk = std::to_string(tid) + "/" + std::to_string(id) + ": le message";
//                tx.send<upstream::chunk>(chunk).get();
//                auto result = rx.recv().get();
//                rx.recv().get();

//                EXPECT_EQ(chunk, *result);
//                if (chunk != *result) {
//                    std::terminate();
//                }
//            }
//        }));

//        for (auto& t : threads) {
//            t.join();
//        }
//    }
//}
