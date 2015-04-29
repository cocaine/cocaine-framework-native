#include "stats.hpp"

#include <cocaine/framework/detail/log.hpp>

using namespace cocaine::framework;

using namespace testing::load;

stats_guard_t::stats_guard_t(size_t iters) :
    stats(boost::accumulators::tag::tail<boost::accumulators::right>::cache_size = iters)
{}

stats_guard_t::~stats_guard_t() {
    for (auto probability : { 0.50, 0.75, 0.90, 0.95, 0.98, 0.99, 0.9995 }) {
        const double val = boost::accumulators::quantile(
            stats,
            boost::accumulators::quantile_probability = probability
        );

        fprintf(stdout, "%6.2f%% : %6.3fms\n", 100 * probability, val);
    }
}

void testing::load::finalize(task<void>::future_move_type future, load_context context) {
    auto now = std::chrono::high_resolution_clock::now();

    auto elapsed = std::chrono::duration<
        double,
        std::chrono::milliseconds::period
    >(now - context.start).count();
    context.stats(elapsed);
    context.counter++;

    future.get();
    CF_DBG("<<< %lu.", context.id + 1);
}


load_context::load_context(size_t id, std::atomic<int>& counter, stats_t& stats) :
    id(id),
    start(std::chrono::high_resolution_clock::now()),
    counter(counter),
    stats(stats)
{
    CF_DBG(">>> %lu.", id + 1);
}
