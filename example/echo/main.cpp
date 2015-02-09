#include <cocaine/framework/worker.hpp>

using namespace cocaine::framework;

int main(int argc, char** argv) {
    worker_t worker(options_t(argc, argv));

    worker.on("ping", [](worker_t::sender_type tx, worker_t::receiver_type rx){
    });

    return worker.run();
}
