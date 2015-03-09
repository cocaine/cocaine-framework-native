#include <cocaine/framework/dispatch.hpp>
#include <cocaine/framework/services/storage.hpp>
#include <cocaine/framework/handlers/http.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>

class App1 {
    struct on_event1 :
        public cocaine::framework::http_handler<App1>,
        public std::enable_shared_from_this<on_event1>
    {
        on_event1(App1 &a) :
            cocaine::framework::http_handler<App1>(a)
        {
            // pass
        }

        void
        on_request(const cocaine::framework::http_request_t& req)
        {
            auto file = parent().m_storage->read("namespace", req.body()).next();

            cocaine::framework::http_headers_t headers;
            headers.add_header("Content-Length", cocaine::format("%d", file.size()));
            response()->write_headers(200, headers); // 200 OK
            response()->write_body(file);
        }
    };
    friend class on_event1;

public:
    App1(cocaine::framework::dispatch_t &d) {
        m_log = d.service_manager()->get_system_logger();
        m_storage = d.service_manager()->get_service<cocaine::framework::storage_service_t>("storage");

        d.on<on_event1>("event1", this);

        COCAINE_LOG_WARNING(m_log, "test log");
    }

private:
    std::shared_ptr<cocaine::framework::logger_t> m_log;
    std::shared_ptr<cocaine::framework::storage_service_t> m_storage;
};

int
main(int argc,
     char *argv[])
{
    return cocaine::framework::run<App1>(argc, argv);
}
