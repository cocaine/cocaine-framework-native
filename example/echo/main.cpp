#include <cocaine/framework/worker.hpp>

using namespace cocaine::framework;

int main(int argc, char** argv) {
    worker_t worker(options_t(argc, argv));

    worker.on("ping", [](worker_session_t::sender_type tx, worker_session_t::receiver_type rx){
        CF_DBG("After invoke");
        std::string message = *rx.recv().get();
        CF_DBG("After chunk: '%s'", message.c_str());
        tx.write(message).get();
        CF_DBG("After write");
    });

    return worker.run();
}
