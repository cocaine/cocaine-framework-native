#include "cocaine/framework/manager.hpp"

using namespace cocaine::framework;

service_manager_t::service_manager_t() :
    work(loop)
{
    auto threads = boost::thread::hardware_concurrency();
    start(threads != 0 ? threads : 1);
}

service_manager_t::service_manager_t(unsigned int threads) :
    work(loop)
{
    if (threads == 0) {
        throw std::invalid_argument("thread count must be a positive number");
    }

    start(threads);
}

service_manager_t::~service_manager_t() {
    work.reset();
    loop.stop();
    pool.join_all();
}

void service_manager_t::start(unsigned int threads) {
    for (unsigned int i = 0; i < threads; ++i) {
        pool.create_thread(
            std::bind(static_cast<std::size_t(loop_t::*)()>(&loop_t::run), std::ref(loop))
        );
    }
}

loop_t& service_manager_t::next() {
    return loop;
}
