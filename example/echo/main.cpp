#include <iostream>

#include <cocaine/framework/worker.hpp>

using namespace cocaine::framework;

int main(int argc, char** argv) {
    worker_t worker(options_t(argc, argv));

    worker.on("ping", [](worker_t::sender_type tx, worker_t::receiver_type rx){
        std::cout << "After invoke" << std::endl;
        std::string message = *rx.recv().get();
        std::cout << "After chunk: '" << message << "'" << std::endl;
        tx.write(message).get();
        std::cout << "After write" << std::endl;
    });

    return worker.run();
}
