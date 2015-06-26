#include <gtest/gtest.h>

#include <cocaine/common.hpp>
#include <cocaine/idl/node.hpp>
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

typedef io::protocol<io::app::enqueue::dispatch_type>::scope scope;

namespace testing { namespace load { namespace app { namespace echo {

task<boost::optional<std::string>>::future_type
on_send(task<channel<io::app::enqueue>::sender_type>::future_move_type future,
        channel<io::app::enqueue>::receiver_type rx)
{
    future.get();
    return rx.recv();
}

task<boost::optional<std::string>>::future_type
on_chunk(task<boost::optional<std::string>>::future_move_type future,
         channel<io::app::enqueue>::receiver_type rx)
{
    auto result = future.get();
    if (!result) {
        throw std::runtime_error("the `result` must be true");
    }
    EXPECT_EQ("le message", *result);

    return rx.recv();
}

void
on_choke(task<boost::optional<std::string>>::future_move_type future) {
    auto result = future.get();
    EXPECT_FALSE(result);
}

task<void>::future_type
on_invoke(task<channel<io::app::enqueue>>::future_move_type future) {
    auto channel = future.get();
    auto tx = std::move(channel.tx);
    auto rx = std::move(channel.rx);
    return tx.send<scope::chunk>(std::string("le message"))
        .then(std::bind(&on_send, ph::_1, rx))
        .then(std::bind(&on_chunk, ph::_1, rx))
        .then(std::bind(&on_choke, ph::_1));
}

} } } } // namespace testing::load::app::echo

TEST(load, app_echo) {
    uint iters        = 100;
    std::string app   = "echo-cpp";
    std::string event = "ping";
    load_config("load.app.echo", iters, app, event);

    std::atomic<int> counter(0);

    stats_guard_t stats(iters);

    service_manager_t manager;
    auto echo = manager.create<io::app_tag>(app);
    echo.connect().get();

    std::vector<task<void>::future_type> futures;
    futures.reserve(iters);

    for (uint id = 0; id < iters; ++id) {
        load_context context(id, counter, stats.stats);

        futures.emplace_back(
            echo.invoke<io::app::enqueue>(std::string(event))
                .then(std::bind(&app::echo::on_invoke, ph::_1))
                .then(std::bind(&finalize, ph::_1, std::move(context)))
        );
    }

    // Block here.
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(iters, counter);
}
