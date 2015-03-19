/*
    Copyright (c) 2015 Evgeny Safronov <division494@gmail.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.
    This file is part of Cocaine.
    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

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
