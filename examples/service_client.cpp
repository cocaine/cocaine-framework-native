#include <cocaine/framework/services/app.hpp>
#include <cocaine/framework/services/storage.hpp>

#include <iostream>

namespace cf = cocaine::framework;

void
handler(cf::future<std::string>& f)
{
    try {
        // Application stub provides raw unpacked response.
        std::cout << "result: " << cocaine::framework::unpack<std::string>(f.get()) << std::endl;
    } catch (const std::exception& e) {
        std::cout << "error: " << e.what() << std::endl;
    }
}

void
read_handler(cf::generator<std::string>& g)
{
    try {
        // Storage always returns packaged binary data, so storage stub unpacks it himself.
        std::cout << "result: " << g.next() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "error: " << e.what() << std::endl;
    }
}

int
main(int argc,
     char *argv[])
{
    auto manager = cf::service_manager_t::create(cf::service_manager_t::endpoint_t("127.0.0.1", 10053));

    // call application
    auto app = manager->get_service<cf::app_service_t>("app1");
    auto g = app->enqueue("event5", "test"); // get generator

    g.map(&handler); // call handler for each chunk

    // or call handler for first chunk:
    // g.then([](cocaine::framework::generator<std::string>& g){std::string result = g.next(); g.map(&handler);});

    // or synchronously get next chunk: std::string result = g.next();

    // call storage service
    auto storage = manager->get_service<cf::storage_service_t>("storage");
    auto g = storage->read("collection", "key"); // get generator

    g.then(&read_handler); // call handler when generator is ready

    return 0;
}
