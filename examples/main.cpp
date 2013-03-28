#include <cocaine/framework/worker.hpp>
#include <cocaine/framework/handlers/functional.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>

class App1 :
    public cocaine::framework::application_t
{
    struct on_event1 :
        public cocaine::framework::user_handler_t<App1>
    {
        on_event1(App1& a) :
            cocaine::framework::user_handler_t<App1>(a)
        {
            // pass
        }

        void
        on_chunk(const char *chunk,
                 size_t size)
        {
            std::cerr << "request: " << std::string(chunk, size) << std::endl;

            std::string buffer("answertestets");
            m_response->write(buffer.data(), buffer.size());
            m_response->close();
        }
    };

public:
    App1(const std::string& name,
         std::shared_ptr<cocaine::framework::service_manager_t> service_manager) :
        application_t(name, service_manager)
    {
        on<on_event1>("event1");
        on("event2", cocaine::framework::method_factory<App1>(this, &App1::on_event2));
        on("event3", cocaine::framework::function_factory(std::bind(&App1::on_event2,
                                                                    this,
                                                                    std::placeholders::_1,
                                                                    std::placeholders::_2)));
    }

    std::string on_event2(const std::string& event,
                          const std::vector<std::string>& input)
    {
        return "on_event2:" + event;
    }
};

int
main(int argc,
     char *argv[])
{
    auto worker = cocaine::framework::make_worker(argc, argv);

    worker->add<App1>("app1");

    worker->run();

    return 0;
}
