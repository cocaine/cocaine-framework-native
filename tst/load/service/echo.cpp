#include <gtest/gtest.h>

#include <cocaine/common.hpp>
#include <cocaine/idl/echo.hpp>
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

namespace testing { namespace load { namespace service { namespace echo {

void
on_invoke(task<std::string>::future_move_type future) {
    auto result = future.get();
    EXPECT_EQ(result, "le message");
}

}}}} // namespace testing::load::service::echo

TEST(load, service_echo) {
    uint iters        = 100;
    std::string app   = "echo";
    std::string event = "ping";
    load_config("load.service.echo", iters, app, event);

    std::atomic<int> counter(0);

    stats_guard_t stats(iters);

    service_manager_t manager;
    auto echo = manager.create<cocaine::io::echo_tag>(app);
    echo.connect().get();

    std::vector<task<void>::future_type> futures;
    futures.reserve(iters);

    for (uint id = 0; id < iters; ++id) {
        load_context context(id, counter, stats.stats);

        futures.emplace_back(
            echo.invoke<io::echo::ping>(std::string("le message"))
                .then(std::bind(&load::service::echo::on_invoke, ph::_1))
                .then(std::bind(&finalize, ph::_1, context))
        );
    }

    // Block here.
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(iters, counter);
}

TEST(load, service_echo_with_timeout) {
    uint iters        = 100;
    std::string app   = "echo";
    std::string event = "ping_with_timeout";
    load_config("load.service.echo_with_timeout", iters, app, event);

    std::atomic<int> counter(0);

    stats_guard_t stats(iters);

    service_manager_t manager;
    auto echo = manager.create<cocaine::io::echo_tag>(app);
    echo.connect().get();

    std::vector<task<void>::future_type> futures;
    futures.reserve(iters);

    for (uint id = 0; id < iters; ++id) {
        load_context context(id, counter, stats.stats);

        futures.emplace_back(
            echo.invoke<io::echo::ping_with_timeout>(std::string("le message"), 100)
                .then(std::bind(&load::service::echo::on_invoke, ph::_1))
                .then(std::bind(&finalize, ph::_1, context))
        );
    }

    // Block here.
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(iters, counter);
}
