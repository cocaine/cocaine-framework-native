#include <gtest/gtest.h>

#include <cocaine/common.hpp>
#include <cocaine/idl/storage.hpp>
#include <cocaine/traits/error_code.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/manager.hpp>

#include "../config.hpp"
#include "../stats.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

using namespace testing;
using namespace testing::load;

namespace testing { namespace load { namespace service { namespace storage {

void
on_invoke(task<std::string>::future_move_type future) {
    auto result = future.get();
    EXPECT_EQ(result, "le value");
}

} } } } // namespace testing::load::service::storage

TEST(load, service_storage) {
    uint iters = 100;
    load_config("load.service.storage", iters);

    std::atomic<int> counter(0);

    stats_guard_t stats(iters);

    service_manager_t manager;
    auto storage = manager.create<cocaine::io::storage_tag>("storage");
    storage.connect().get();

    std::vector<task<void>::future_type> futures;
    futures.reserve(iters);

    for (uint id = 0; id < iters; ++id) {
        load_context context(id, counter, stats.stats);

        futures.emplace_back(
            storage.invoke<io::storage::read>(std::string("collection"), std::string("key"))
                .then(std::bind(&load::service::storage::on_invoke, ph::_1))
                .then(std::bind(&finalize, ph::_1, std::ref(context)))
        );
    }

    // Block here.
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(iters, counter);
}


