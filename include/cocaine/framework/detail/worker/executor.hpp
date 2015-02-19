#pragma once

#include <boost/optional.hpp>
#include <boost/thread/thread.hpp>

#include "cocaine/framework/detail/loop.hpp"

namespace cocaine {

namespace framework {

namespace detail {

namespace worker {

/*!
 * RAII thread pool executor
 *
 * \internal
 */
class executor_t {
    detail::loop_t loop;
    boost::optional<detail::loop_t::work> work;
    boost::thread_group pool;

public:
    executor_t() :
        work(boost::optional<detail::loop_t::work>(detail::loop_t::work(loop)))
    {
        auto threads = boost::thread::hardware_concurrency();
        start(threads != 0 ? threads : 1);
    }

    explicit executor_t(unsigned int threads) :
        work(boost::optional<detail::loop_t::work>(detail::loop_t::work(loop)))
    {
        if (threads == 0) {
            throw std::invalid_argument("thread count must be a positive number");
        }

        start(threads);
    }

    ~executor_t() {
        work.reset();
        pool.join_all();
    }

    void operator()(std::function<void()> fn) {
        loop.post(std::move(fn));
    }

private:
    void start(unsigned int threads) {
        for (unsigned int i = 0; i < threads; ++i) {
            pool.create_thread(
                std::bind(static_cast<std::size_t(detail::loop_t::*)()>(&detail::loop_t::run), std::ref(loop))
            );
        }
    }
};

}

}

}

}
