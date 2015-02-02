#include <gtest/gtest.h>

#include <cocaine/idl/storage.hpp>
#include <cocaine/idl/streaming.hpp>
#include <cocaine/idl/node.hpp>

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
    auto result = storage.invoke<cocaine::io::storage::read>(std::string("collection"), std::string("key")).get();

    EXPECT_EQ("le value", result);
}

TEST(service, StorageError) {
    client_t client;

    service<cocaine::io::storage_tag> storage("storage", client.loop());
    EXPECT_THROW(storage.invoke<cocaine::io::storage::read>(std::string("i-collection"), std::string("key")).get(), cocaine_error);
}

TEST(service, Echo) {
    typedef typename cocaine::io::protocol<cocaine::io::app::enqueue::dispatch_type>::scope upstream;

    client_t client;
    service<cocaine::io::app_tag> echo("echo", client.loop());
    auto ch = echo.invoke<cocaine::io::app::enqueue>(std::string("ping")).get();
    auto tx = std::move(std::get<0>(ch));
    auto rx = std::move(std::get<1>(ch));
    tx.send<upstream::chunk>(std::string("le message")).get();
    auto result = rx.recv().get();

    EXPECT_EQ("le message", *result);
}
