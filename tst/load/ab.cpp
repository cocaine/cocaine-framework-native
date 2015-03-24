#include <gtest/gtest.h>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/density.hpp>

#include <cocaine/idl/node.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/manager.hpp>

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

typedef io::protocol<io::app::enqueue::dispatch_type>::scope scope;

typedef boost::accumulators::accumulator_set<
    double,
    boost::accumulators::stats<
        boost::accumulators::tag::tail_quantile<boost::accumulators::right>
    >
> acc_t;

namespace ab {

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
    if (!result) {
        throw std::runtime_error("`result` must be true");
    }
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

void on_end(task<void>::future_move_type future, std::chrono::high_resolution_clock::time_point start, uint i, acc_t& acc) {
    auto now = std::chrono::high_resolution_clock::now();

    auto elapsed = std::chrono::duration<float, std::chrono::milliseconds::period>(now - start).count();
    acc(elapsed);
    future.get();
}

} // namespace ab

#include <asio.hpp>
#include <boost/thread.hpp>
#include <cocaine/framework/detail/loop.hpp>

TEST(load, ab) {
    const uint ITERS = 2000;

    acc_t acc(boost::accumulators::tag::tail<boost::accumulators::right>::cache_size = ITERS);

    std::atomic<int> counter(0);

    service_manager_t manager(1);
    auto echo = manager.create<cocaine::io::app_tag>("echo-cpp");
    echo.connect().get();

    std::vector<task<void>::future_type> futures;
    futures.reserve(ITERS);

    for (uint i = 0; i < ITERS; ++i) {
        auto now = std::chrono::high_resolution_clock::now();
        futures.emplace_back(
            echo.invoke<io::app::enqueue>(std::string("ping"))
                .then(std::bind(&ab::on_invoke, ph::_1, std::ref(counter)))
                .then(std::bind(&ab::on_end, ph::_1, now, i, std::ref(acc)))
        );
    }

    // Block here.
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(ITERS, counter);

    for (auto probability : { 0.50, 0.75, 0.90, 0.95, 0.98, 0.99, 0.995 }) {
        const double val = boost::accumulators::quantile(
            acc,
            boost::accumulators::quantile_probability = probability
        );

        fprintf(stdout, "%6.3f : %6.3f\n", probability, val);
    }
}
