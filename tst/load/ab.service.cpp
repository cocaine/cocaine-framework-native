#include <boost/thread/thread.hpp>

#include <gtest/gtest.h>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/manager.hpp>

#include "../util/env.hpp"

#include <cocaine/idl/echo.hpp>

TEST(load, ab) {
    const std::string CONFIG_FILENAME = testing::util::get_option<std::string>("CF_CFG", "");

    std::string app = "echo";
    std::string event = "ping";
    uint iters = 100;

    if (!CONFIG_FILENAME.empty()) {
        std::ifstream infile(CONFIG_FILENAME);
        infile >> app >> event >> iters;
    }

    acc_t acc(boost::accumulators::tag::tail<boost::accumulators::right>::cache_size = iters);

    std::atomic<int> counter(0);

    {
        service_manager_t manager(7);
        auto echo = manager.create<cocaine::io::echo_tag>(app);
        echo.connect().get();

        std::vector<task<void>::future_type> futures;
        futures.reserve(iters);

        for (uint id = 0; id < iters; ++id) {
            auto now = std::chrono::high_resolution_clock::now();
            CF_DBG(">>> %d.", id + 1);
            futures.emplace_back(
                echo.invoke<io::echo::ping>(std::string("le message"))
                    .then(std::bind(&ab::echo::on_invoke, ph::_1))
                    .then(std::bind(&ab::on_end, ph::_1, now, id, std::ref(acc), std::ref(counter)))
            );
        }

        // Block here.
        for (auto& future : futures) {
            future.get();
        }
    }

    EXPECT_EQ(iters, counter);

    for (auto probability : { 0.50, 0.75, 0.90, 0.95, 0.98, 0.99, 0.9995 }) {
        const double val = boost::accumulators::quantile(
            acc,
            boost::accumulators::quantile_probability = probability
        );

        fprintf(stdout, "%6.2f%% : %6.3fms\n", 100 * probability, val);
    }
}
