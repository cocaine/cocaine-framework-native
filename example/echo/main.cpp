#include <cocaine/framework/worker.hpp>

using namespace cocaine::framework;

int main(int argc, char** argv) {
    worker_t worker(options_t(argc, argv));

    worker.on("ping", [](worker_session_t::sender_type tx, worker_session_t::receiver_type rx){
        try {
        std::cout << "Invoke!" << std::endl;
        std::string message = *rx.recv().get();
        std::cout << "Le message:" << message << std::endl;
        tx.write(message).get();
        std::cout << "After send" << std::endl;
        } catch (std::runtime_error e) {
            std::cout << e.what() << std::endl;
        }
    });

    return worker.run();
}
