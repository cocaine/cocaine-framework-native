#include <gtest/gtest.h>

#include <cocaine/common.hpp>
#include <cocaine/idl/storage.hpp>
#include <cocaine/traits/error_code.hpp>

#include <cocaine/framework/manager.hpp>
#include <cocaine/framework/service.hpp>

using namespace cocaine::framework;

TEST(manual, DISABLED_StorageAutoReconnect) {
    service_manager_t manager(1);
    auto storage = manager.create<cocaine::io::storage_tag>("storage");
    auto result = storage.invoke<cocaine::io::storage::read>("collection", "key").get();

    EXPECT_EQ("le value", result);
    sleep(5);

    result = storage.invoke<cocaine::io::storage::read>("collection", "key").get();

    EXPECT_EQ("le value", result);
}
