#include <iostream>

#include <boost/optional.hpp>

#include <cocaine/framework/worker.hpp>

using namespace cocaine::framework;

int main(int argc, char** argv) {
    worker_t worker(options_t(argc, argv));

    worker.on("version", [](worker::sender tx, worker::receiver) {
        tx.write("1.0").get();
    });

    worker.on("ping", [](worker::sender tx, worker::receiver rx) {
        std::cout << "After invoke" << std::endl;
        if (boost::optional<std::string> message = rx.recv().get()) {
            std::cout << "After chunk: '" << *message << "'" << std::endl;
            tx.write(*message).get();
            std::cout << "After write" << std::endl;
        }

        std::cout << "After close" << std::endl;
    });

    worker.on("meta", [](worker::sender tx, worker::receiver rx) {
        std::cout << "After invoke. Headers count: "
            << rx.invocation_headers().get_headers().size() << std::endl;

        if (auto frame = rx.recv<worker::frame_t>().get()) {
            std::cout << "After chunk: '" << frame->data << "'" << std::endl;
            tx.write(frame->data).get();
            std::cout << "After write" << std::endl;
        }

        std::cout << "After close" << std::endl;
    });

    return worker.run();
}
