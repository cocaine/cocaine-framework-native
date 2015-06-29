#pragma once

#include <atomic>
#include <chrono>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/density.hpp>

#include <cocaine/framework/forwards.hpp>

namespace testing { namespace load {

typedef boost::accumulators::accumulator_set<
    double,
    boost::accumulators::stats<
        boost::accumulators::tag::extended_p_square
    >
> stats_t;

struct load_context {
    size_t id;
    std::chrono::high_resolution_clock::time_point start;
    std::atomic<int>& counter;
    stats_t& stats;

    load_context(size_t id, std::atomic<int>& counter, stats_t& stats);
};

class stats_guard_t {
public:
    stats_t stats;

    explicit stats_guard_t(size_t iters);
    ~stats_guard_t();
};

void finalize(cocaine::framework::task<void>::future_move_type future, load_context context);

}} // namespace testing::load
