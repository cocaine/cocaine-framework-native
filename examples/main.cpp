#include <cocaine/framework/worker.hpp>
#include <cocaine/framework/application.hpp>
#include <cocaine/framework/services/logger.hpp>
#include <cocaine/framework/services/storage.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>

std::string
on_event3(const std::string& event,
          const std::vector<std::string>& input)
{
    std::cout << "on_event3:" + event << std::endl;
    return "on_event3:" + event;
}

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

            std::string buffer("answertestets");
            response()->write(buffer.data(), buffer.size());
            response()->close();
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
        }
    };
    friend class on_event1;

public:
    App1(const std::string& name,
         std::shared_ptr<cocaine::framework::service_manager_t> service_manager) :
        cocaine::framework::application<App1>(name, service_manager)
    {
        // pass
    }

    void
    initialize() {
        m_log = service_manager()->get_system_logger();
        create_service(m_storage, "storage");

        on<on_event1>("event1");
        on("event2", &App1::on_event2);
        on("event3", &on_event3);

        COCAINE_LOG_WARNING(m_log, "test log");
    }

    std::string
    on_event2(const std::string& event,
              const std::vector<std::string>& input)
    {
        std::cout << "on_event2:" + event << std::endl;
        return "on_event2:" + event;
    }

private:
    std::shared_ptr<cocaine::framework::logging_service_t> m_log;
    std::shared_ptr<cocaine::framework::storage_service_t> m_storage;
};

int
main(int argc,
     char *argv[])
{
    return cocaine::framework::worker_t::run<App1>(argc, argv);
}
