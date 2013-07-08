#include <cocaine/framework/services/app.hpp>
#include <cocaine/framework/services/storage.hpp>

#include <iostream>

namespace cf = cocaine::framework;

void
handler(cf::future<std::string>& f)
{
    try {
        std::cout << "result: " << f.get() << std::endl;
    } catch (std::exception e) {
        std::cout << "error: " << e.what() << std::endl;
    }
}

int
main(int argc,
     char *argv[])
{
    auto manager = cf::service_manager_t::create(cocaine::io::tcp::endpoint("127.0.0.1", 10053));

    // call application
    auto app = manager->get_service<cf::app_service_t>("app1");
    auto g = app->enqueue("event5", "test"); // get generator

    g.map(&handler); // call handler for each chunk

    // or do call for first chunk:
    // g.then([](cocaine::framework::generator<std::string>& g){std::string result = g.next(); g.map(&handler);});

    // or synchronously get next chunk: std::string result = g.next();

    // call storage service
    auto storage = manager->get_service<cf::storage_service_t>("storage");
    auto f = storage->read("collection", "key"); // get future

    f.then(&handler); // call handler when future is ready
    // or std::string result = f.get();

    return 0;
}
