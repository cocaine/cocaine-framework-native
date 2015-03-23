#include <boost/thread/thread.hpp>

#include <gtest/gtest.h>

#include <cocaine/idl/node.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/manager.hpp>

#include "../util/env.hpp"

using namespace cocaine;
using namespace cocaine::framework;

using namespace testing;

typedef typename io::protocol<io::app::enqueue::dispatch_type>::scope scope;

static const char DEFAULT_APP[] = "echo-cpp";
static const uint DEFAULT_ITERS = 10;

TEST(load, EchoSyncST) {
    const std::string APP = util::get_option<std::string>("load.EchoSyncST.app", DEFAULT_APP);
    const uint ITERS = util::get_option<uint>("load.EchoSyncST.iters", DEFAULT_ITERS);

    service_manager_t manager(1);
    auto echo = manager.create<cocaine::io::app_tag>(APP);

    for (uint id = 0; id < ITERS; ++id) {
        auto channel = echo.invoke<io::app::enqueue>(std::string("ping")).get();
        auto tx = std::move(channel.tx);
        auto rx = std::move(channel.rx);

        tx.send<scope::chunk>(std::string("le message")).get();

        auto result = rx.recv().get();
        EXPECT_EQ("le message", *result);

        auto choke = rx.recv().get();
        EXPECT_FALSE(choke);
    }
}

namespace ph = std::placeholders;

namespace {

task<boost::optional<std::string>>::future_type
on_send(task<sender<io::app::enqueue::dispatch_type, basic_session_t>>::future_move_type future,
        receiver<io::app::enqueue::upstream_type, basic_session_t> rx)
{
    future.get();
    return rx.recv();
}

task<boost::optional<std::string>>::future_type
on_chunk(task<boost::optional<std::string>>::future_move_type future,
        receiver<io::app::enqueue::upstream_type, basic_session_t> rx)
{
    auto result = future.get();
    EXPECT_EQ("le message", *result);

    return rx.recv();
}

void
on_choke(task<boost::optional<std::string>>::future_move_type future, std::atomic<int>& counter) {
    auto result = future.get();
    EXPECT_FALSE(result);

    counter++;
}

task<void>::future_type
on_invoke(task<invocation_result<io::app::enqueue>::type>::future_move_type future,
          std::atomic<int>& counter)
{
    auto channel = future.get();
    auto tx = std::move(channel.tx);
    auto rx = std::move(channel.rx);
    return tx.send<scope::chunk>(std::string("le message"))
        .then(std::bind(&on_send, ph::_1, rx))
        .then(std::bind(&on_chunk, ph::_1, rx))
        .then(std::bind(&on_choke, ph::_1, std::ref(counter)));
}

} // namespace

TEST(load, EchoAsyncST) {
    const std::string APP = util::get_option<std::string>("load.EchoAsyncST.app", DEFAULT_APP);
    const uint ITERS = util::get_option<uint>("load.EchoAsyncST.iters", DEFAULT_ITERS);

    std::atomic<int> counter(0);

    service_manager_t manager(1);
    auto echo = manager.create<cocaine::io::app_tag>(APP);

    std::vector<task<void>::future_type> futures;
    futures.reserve(ITERS);

    for (uint i = 0; i < ITERS; ++i) {
        futures.emplace_back(
            echo.invoke<io::app::enqueue>(std::string("ping"))
                .then(std::bind(&on_invoke, ph::_1, std::ref(counter)))
        );
    }

    // Block here.
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(ITERS, counter);
}

}
