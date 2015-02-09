#include <cocaine/framework/worker.hpp>

using namespace cocaine::framework;

int main(int argc, char** argv) {
    worker_t worker(options_t(argc, argv));

    worker.on("ping", [](worker_session_t::sender_type tx/*, worker_session_t::receiver_type rx*/){
        std::cout << "Invoke!" << std::endl;
    });

    return worker.run();
}
