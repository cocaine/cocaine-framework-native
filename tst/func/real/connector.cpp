#include <boost/asio/ip/tcp.hpp>

#include <gtest/gtest.h>

#include <cocaine/framework/forwards.hpp>
#include <cocaine/framework/scheduler.hpp>

#include <cocaine/framework/detail/log.hpp>
#include <cocaine/framework/detail/loop.hpp>
#include <cocaine/framework/detail/resolver.hpp>

#include "../../util/net.hpp"

using namespace testing;
using namespace testing::util;

using namespace cocaine::framework;
using namespace cocaine::framework::detail;

TEST(Resolver, Resolve) {
    client_t client;
    event_loop_t loop { client.loop() };
    scheduler_t scheduler(loop);

    resolver_t resolver(scheduler);
    auto result = resolver.resolve("storage").get();

    EXPECT_FALSE(result.endpoints.empty());
    EXPECT_EQ(1, result.version);
}
