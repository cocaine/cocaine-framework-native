#include "cocaine/framework/manager.hpp"

#include <boost/optional.hpp>
#include <boost/thread/thread.hpp>

#include "cocaine/framework/scheduler.hpp"

#include "cocaine/framework/detail/loop.hpp"

using namespace cocaine::framework;
using namespace cocaine::framework::detail;

class cocaine::framework::execution_unit_t {
public:
    loop_t io;
    event_loop_t event_loop;
    scheduler_t scheduler;
    boost::thread thread;
    boost::optional<loop_t::work> work;

    execution_unit_t() :
        event_loop(io),
        scheduler(event_loop),
        thread(std::bind(static_cast<std::size_t(loop_t::*)()>(&loop_t::run), std::ref(io))),
        work(boost::optional<loop_t::work>(loop_t::work(io)))
    {}

    ~execution_unit_t() {
        work.reset();
        thread.join();
    }
};

service_manager_t::service_manager_t() :
    current(0)
{
    auto threads = boost::thread::hardware_concurrency();
    start(threads != 0 ? threads : 1);
}

service_manager_t::service_manager_t(unsigned int threads) :
    current(0)
{
    if (threads == 0) {
        throw std::invalid_argument("thread count must be a positive number");
    }

    start(threads);
}

service_manager_t::~service_manager_t() {
    units.clear();
}

void service_manager_t::start(unsigned int threads) {
    for (unsigned int i = 0; i < threads; ++i) {
        units.emplace_back(new execution_unit_t);
    }
}

scheduler_t& service_manager_t::next() {
    if (current >= units.size()) {
        current = 0;
    }

    return units[current]->scheduler;
}
