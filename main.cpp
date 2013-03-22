#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>
#include <msgpack.hpp>
#include <crypto++/cryptlib.h>
#include <crypto++/sha.h>
#include <cocaine/framework/worker.hpp>
#include <cocaine/framework/factories.hpp>

using namespace cocaine::framework;

class App1 :
    public cocaine::framework::application_t
{
    struct on_event1 :
        public cocaine::framework::user_handler_t<App1>
    {
        on_event1(App1& a) :
            user_handler_t<App1>(a)
        {
            // pass
        }

        void
        write(const char *chunk,
             size_t size)
        {
            msgpack::sbuffer buffer;
            msgpack::packer<msgpack::sbuffer> pk(&buffer);
            pk.pack_map(2);
            pk.pack(std::string("code"));
            pk.pack(200);

            std::vector<std::pair<std::string, std::string>> headers;
            headers.push_back(std::make_pair(std::string("Content-Type"), std::string("text/plain")));

            pk.pack(std::string("headers"));
            pk.pack(headers);

            m_response->write(buffer.data(), buffer.size());

            std::string msg = "jvbherjhvhejrhbgvehjrbgvhjerbvgherbvgherb";
            byte abd[CryptoPP::SHA512::DIGESTSIZE];
            CryptoPP::SHA512().CalculateDigest(abd, (const byte*)msg.data(), msg.size());

            std::string body = "<html><body>" + m_event + "</body></html>";

            msgpack::pack(buffer, body);
            m_response->write(buffer.data(), buffer.size());
            m_response->close();
        }
    };

    struct on_exit :
        public cocaine::framework::user_handler_t<App1>
    {
        on_exit(App1& a) :
            user_handler_t<App1>(a)
        {
            // pass
        }

        void
        write(const char *chunk,
             size_t size)
        {
            std::cerr << "on_exit" << std::endl;
            m_response->close();
            // exit(0);
        }
    };

public:
    App1() {
        on<on_event1>("event1");
        on("event2", cocaine::framework::method_factory_t<App1>(&App1::on_event2));
        on("event3", cocaine::framework::function_factory(std::bind(&App1::on_event2,
                                                this,
                                                std::placeholders::_1,
                                                std::placeholders::_2)));
        on<on_exit>("exit");
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

    worker->add("app1", App1());

    worker->run();

    return 0;
}
