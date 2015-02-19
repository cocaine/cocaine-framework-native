#include "cocaine/framework/manager.hpp"

#include "cocaine/framework/scheduler.hpp"

#include "cocaine/framework/detail/loop.hpp"

using namespace cocaine::framework;

class cocaine::framework::execution_unit_t {
public:
    detail::loop_t loop;
    event_loop_t event_loop;
    scheduler_t scheduler;
    boost::optional<detail::loop_t::work> work;

    execution_unit_t() :
        event_loop(loop),
        scheduler(event_loop),
        work(boost::optional<detail::loop_t::work>(detail::loop_t::work(loop)))
    {}

    ~execution_unit_t() {
        work.reset();
        loop.stop();
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
    pool.join_all();
}

void service_manager_t::start(unsigned int threads) {
    for (unsigned int i = 0; i < threads; ++i) {
        std::unique_ptr<execution_unit_t> unit(new execution_unit_t);
        pool.create_thread(
            std::bind(static_cast<std::size_t(detail::loop_t::*)()>(&detail::loop_t::run), std::ref(unit->loop))
        );
        units.push_back(std::move(unit));
    }
}

scheduler_t& service_manager_t::next() {
    if (current >= units.size()) {
        current = 0;
    }

    return units[current]->scheduler;
}
