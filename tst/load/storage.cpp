#include <future>
#include <thread>

#include <boost/thread/thread.hpp>

#include <gtest/gtest.h>

#include <cocaine/idl/storage.hpp>

#include <cocaine/framework/manager.hpp>
#include <cocaine/framework/service.hpp>

#include "../util/env.hpp"

using namespace cocaine;
using namespace cocaine::framework;

using namespace testing;

static const uint DEFAULT_ITERS = 10;

TEST(load, StorageAsyncST) {
    const uint ITERS = util::get_option<uint>("load.StorageAsyncST", DEFAULT_ITERS);

    service_manager_t manager(1);
    auto storage = manager.create<cocaine::io::storage_tag>("storage");

    std::vector<typename task<std::string>::future_type> futures;
    futures.reserve(ITERS);

    for (int i = 0; i < ITERS; ++i) {
        futures.emplace_back(
            storage.invoke<io::storage::read>(std::string("collection"), std::string("key"))
        );
    }

    // Block here.
    for (auto& future : futures) {
        EXPECT_EQ("le value", future.get());
    }
}

TEST(load, StorageAsyncMT) {
    const uint ITERS = util::get_option<uint>("load.StorageAsyncMT", DEFAULT_ITERS);

    service_manager_t manager(4);
    std::vector<boost::thread> threads;
    std::vector<std::future<void>> futures;

    for (int tid = 0; tid < 4; ++tid) {
        std::packaged_task<void()> work([&manager, ITERS]{
            auto storage = manager.create<cocaine::io::storage_tag>("storage");
            std::vector<typename task<std::string>::future_type> futures;
            futures.reserve(ITERS);

            for (int i = 0; i < ITERS; ++i) {
                futures.emplace_back(
                    storage.invoke<io::storage::read>(std::string("collection"), std::string("key"))
                );
            }

            // Block here.
            for (auto& future : when_all(futures).get()) {
                EXPECT_EQ("le value", future.get());
            }
        });

        futures.emplace_back(work.get_future());
        threads.emplace_back(std::move(work));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (auto& future : futures) {
        future.get();
    }
}
