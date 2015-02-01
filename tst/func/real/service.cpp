#include <gtest/gtest.h>

#include <cocaine/idl/storage.hpp>

#include <cocaine/framework/service.hpp>

#include "../../util/net.hpp"

using namespace cocaine::framework;

using namespace testing;
using namespace testing::util;

TEST(service, Storage) {
    // Helper object, that runs event loop in the separate thread.
    client_t client;

    // Service's constructor does nothing.
    service<cocaine::io::storage_tag> storage("storage", client.loop());

    // Invoke (and internally connect) read method.
    auto result = storage.invoke<cocaine::io::storage::read>("collection", "key").get();

    EXPECT_EQ("le value", result);
}
