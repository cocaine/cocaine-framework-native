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

#include "cocaine/framework/manager.hpp"

#include <boost/optional.hpp>
#include <boost/thread/thread.hpp>

#include <cocaine/idl/logging.hpp>

#include "cocaine/framework/scheduler.hpp"
#include "cocaine/framework/service.hpp"

#include "cocaine/framework/detail/loop.hpp"
#include "cocaine/framework/detail/runnable.hpp"

namespace io {
    using cocaine::io::log_tag;
} // namespace io

using namespace cocaine::framework;
using namespace cocaine::framework::detail;

namespace {

class execution_unit_t {
public:
    loop_t io;

    boost::optional<loop_t::work> work;
    event_loop_t event_loop;
    scheduler_t scheduler;
    boost::thread thread;

    execution_unit_t() :
        work(boost::optional<loop_t::work>(loop_t::work(io))),
        event_loop(io),
        scheduler(event_loop),
        thread(named_runnable<loop_t>("[CF::M]", io))
    {}

    ~execution_unit_t() {
        work.reset();
        thread.join();
    }
};

} // namespace

static const std::vector<session_t::endpoint_type> DEFAULT_LOCATIONS = {
    { boost::asio::ip::tcp::v6(), 10053 }
};

class cocaine::framework::service_manager_data {
public:
    loop_t io;
    boost::optional<loop_t::work> work;
    event_loop_t event_loop;
    scheduler_t scheduler;

    std::vector<session_t::endpoint_type> locations;

    std::vector<boost::thread> threads;

    std::shared_ptr<service<io::log_tag>> logger;

    service_manager_data(std::vector<session_t::endpoint_type> locations_) :
        work(boost::optional<loop_t::work>(loop_t::work(io))),
        event_loop(io),
        scheduler(event_loop),
        locations(std::move(locations_)),
        logger(std::make_shared<service<io::log_tag>>("logging", locations, scheduler))
    {}
};

service_manager_t::service_manager_t() :
    d(new service_manager_data(DEFAULT_LOCATIONS))
{
    const auto threads = boost::thread::hardware_concurrency();
    start(threads != 0 ? threads : 1);
}

service_manager_t::service_manager_t(unsigned int threads):
    d(new service_manager_data(DEFAULT_LOCATIONS))
{
    if (threads > 0) {
        start(threads);
    } else {
        throw std::invalid_argument("thread count must be a positive number");
    }
}

service_manager_t::service_manager_t(std::vector<endpoint_type> entries, unsigned int threads):
    d(new service_manager_data(std::move(entries)))
{
    if (threads > 0) {
        start(threads);
    } else {
        throw std::invalid_argument("thread count must be a positive number");
    }
}

service_manager_t::~service_manager_t() {
    // Reset an own copy of a logger shared pointer to be able to join threads gracefully.
    // Otherwise they will wait forever until all asynchronous operations completes.
    d->logger.reset();

    d->work.reset();

    for (auto& thread : d->threads) {
        thread.join();
    }
}

std::vector<session_t::endpoint_type>
service_manager_t::endpoints() const {
    return d->locations;
}

void
service_manager_t::start(unsigned int threads) {
    for (unsigned int i = 0; i < threads; ++i) {
        d->threads.emplace_back(named_runnable<loop_t>("[CF::M]", d->io));
    }
}

scheduler_t&
service_manager_t::next() {
    return d->scheduler;
}

std::shared_ptr<service<io::log_tag>>
service_manager_t::logger() const {
    return d->logger;
}
