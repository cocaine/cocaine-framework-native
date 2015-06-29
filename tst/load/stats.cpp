#include "stats.hpp"

#include <cocaine/framework/detail/log.hpp>

using namespace cocaine::framework;

using namespace testing::load;

static const std::vector<double> probabilities_ = {
    { 0.50, 0.75, 0.90, 0.95, 0.98, 0.99, 0.9995 }
};

inline
double
trunc(double v, uint n) noexcept {
    BOOST_ASSERT(n < 10);

    static const long long table[10] = {
        1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000
    };

    return std::ceil(v * table[n]) / table[n];
}

stats_guard_t::stats_guard_t(size_t iters) :
    stats(boost::accumulators::tag::extended_p_square::probabilities = probabilities_)
{}

stats_guard_t::~stats_guard_t() {
    const auto& probs = probabilities_;

    for (std::size_t i = 0; i < probs.size(); ++i) {
        const auto quantile = boost::accumulators::extended_p_square(stats)[i];
        fprintf(stdout, "%6.2f%% : %6.3fms\n", trunc(100 * probs[i], 2), trunc(quantile, 3));
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
    CF_DBG("<<< %llu.", CF_US(context.id + 1));
}


load_context::load_context(size_t id, std::atomic<int>& counter, stats_t& stats) :
    id(id),
    start(std::chrono::high_resolution_clock::now()),
    counter(counter),
    stats(stats)
{
    CF_DBG(">>> %llu.", CF_US(id + 1));
}
