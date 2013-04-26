#include <cocaine/framework/worker.hpp>
#include <cocaine/framework/application.hpp>
#include <cocaine/framework/services/storage.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>

class App1 :
    public cocaine::framework::application<App1>
{
    struct on_event1 :
        public cocaine::framework::handler<App1>,
        public std::enable_shared_from_this<on_event1>
    {
        on_event1(std::shared_ptr<App1> a) :
            cocaine::framework::handler<App1>(a)
        {
            // pass
        }

        void
        on_chunk(const char *chunk,
                 size_t size)
        {
            std::cout << "request: " << std::string(chunk, size) << std::endl;

            auto error_handler = std::bind(&on_event1::on_error,
                                           shared_from_this(),
                                           std::placeholders::_1,
                                           std::placeholders::_2);

            app()->m_storage->read("testtest", "testkey")
                .on_message(std::bind(&on_event1::on_read, shared_from_this(), std::placeholders::_1))
                .on_error(error_handler);
        }

        void
        on_error(int code,
                 const std::string& message)
        {
            std::cerr << "ERROR: " << code << ", " << message << std::endl;
            COCAINE_LOG_ERROR(app()->m_log, "ERROR: %d: %s", code, message);
        }

        void
        on_read(const std::string& value) {
            std::cout << "on_read: " << value << std::endl;
            response()->write(value);
            response()->close();
        }
    };
    friend class on_event1;

public:
    App1(std::shared_ptr<cocaine::framework::service_manager_t> service_manager) :
        cocaine::framework::application<App1>(service_manager)
    {
        // pass
    }

    void
    initialize() {
        m_log = service_manager()->get_system_logger();
        create_service(m_storage, "storage");

        on<on_event1>("event1");
        on("event2", &App1::on_event2);

        COCAINE_LOG_WARNING(m_log, "test log");
    }

    void
    on_event2(const std::string& event,
              const std::vector<std::string>& request,
              cocaine::framework::response_ptr response)
    {
        std::cout << "on_event2:" + event << std::endl;
        response->write("on_event2:" + event);
    }

private:
    std::shared_ptr<cocaine::framework::logger_t> m_log;
    std::shared_ptr<cocaine::framework::storage_service_t> m_storage;
};

int
main(int argc,
     char *argv[])
{
    return cocaine::framework::worker_t::run<App1>(argc, argv);
}
