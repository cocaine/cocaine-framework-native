#include <iostream>

#include <boost/optional.hpp>

#include <cocaine/framework/worker.hpp>
#include <cocaine/framework/worker/http.hpp>

using namespace cocaine::framework;
using namespace cocaine::framework::worker;

int main(int argc, char** argv) {
    worker_t worker(options_t(argc, argv));

    typedef http::event<> http_t;
    worker.on<http_t>("http", [](http_t::fresh_sender tx, http_t::fresh_receiver){
        http_response_t rs;
        rs.code = 200;
        tx.send(std::move(rs)).get()
            .send("Hello from C++").get();
    });

    return worker.run();
}
