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

future<boost::optional<std::string>>
on_send(future<channel<io::app::enqueue>::sender_type>& fr,
        channel<io::app::enqueue>::receiver_type rx)
{
    auto tx = fr.get();
    tx.send<scope::choke>();
    return rx.recv();
}

future<boost::optional<std::string>>
on_chunk(future<boost::optional<std::string>>& fr,
         channel<io::app::enqueue>::receiver_type rx)
{
    auto result = fr.get();
    if (!result) {
        throw std::runtime_error("the `result` must be true");
    }
    EXPECT_EQ("le message", *result);

    return rx.recv();
}

void
on_choke(future<boost::optional<std::string>>& fr) {
    auto result = fr.get();
    EXPECT_FALSE(result);
}

future<void>
on_invoke(future<channel<io::app::enqueue>>& fr) {
    auto channel = fr.get();

    channel.tx.send<scope::chunk>(std::string("le message"))
        .then([](future<framework::channel<io::app::enqueue>::sender_type>& fr2)
    {
        auto tx = fr2.get();
        tx.send<scope::choke>();
    });

    auto rx = std::move(channel.rx);
    return rx.recv()
        .then(std::bind(&on_chunk, ph::_1, rx))
        .then(std::bind(&on_choke, ph::_1));
}

}}}} // namespace testing::load::app::echo

TEST(load, app_echo) {
    uint iters        = 100;
    std::string app   = "echo-cpp";
    std::string event = "ping";
    load_config("load.app.echo", iters, app, event);

    std::atomic<int> counter(0);

    stats_guard_t stats(iters);

    service_manager_t manager(4);
    auto echo = manager.create<io::app_tag>(app);
    echo.connect().get();

    std::vector<future<void>> futures;
    futures.reserve(iters);

    const auto now = std::chrono::high_resolution_clock::now();
    for (uint id = 0; id < iters; ++id) {
        load_context context(id, counter, stats.stats);

        futures.emplace_back(
            echo.invoke<io::app::enqueue>(std::string(event))
                .then(std::bind(&app::echo::on_invoke, ph::_1))
                .then(std::bind(&finalize, ph::_1, std::move(context)))
        );
    }
    std::cout << std::chrono::duration_cast<
        std::chrono::milliseconds
    >(std::chrono::high_resolution_clock::now() - now).count() << " ms" << std::endl;

    // Block here.
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(iters, counter);
}


namespace testing { namespace load { namespace app { namespace echo { namespace version {

void
on_chunk(future<boost::optional<std::string>>& fr) {
    if (auto result = fr.get()) {
        EXPECT_EQ("1.0", *result);
    } else {
        throw std::runtime_error("the `result` must be true");
    }
}

future<void>
on_invoke(future<channel<io::app::enqueue>>& fr) {
    auto ch = fr.get();
    ch.tx.send<scope::choke>();

    return ch.rx.recv()
        .then(std::bind(&on_chunk, ph::_1));
}

}}}}} // namespace testing::load::app::echo::version

TEST(load, app_version) {
    uint iters        = 100;
    std::string app   = "echo-cpp";
    std::string event = "version";
    load_config("load.app.echo", iters, app, event);

    std::atomic<int> counter(0);

    stats_guard_t stats(iters);

    service_manager_t manager;
    auto echo = manager.create<io::app_tag>(app);
    echo.connect().get();

    std::vector<future<void>> futures;
    futures.reserve(iters);

    for (uint id = 0; id < iters; ++id) {
        load_context context(id, counter, stats.stats);

        futures.emplace_back(
            echo.invoke<io::app::enqueue>(std::string(event))
                .then(std::bind(&app::echo::version::on_invoke, ph::_1))
                .then(std::bind(&finalize, ph::_1, std::move(context)))
        );
    }

    // Block here.
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(iters, counter);
}
