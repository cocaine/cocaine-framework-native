#include <thread>

#include <asio.hpp>

#include <cocaine/api/connect.hpp>
#include <cocaine/common.hpp>
#include <cocaine/idl/locator.hpp>
#include <cocaine/idl/node.hpp>
#include <cocaine/traits.hpp>
#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/graph.hpp>
#include <cocaine/traits/map.hpp>
#include <cocaine/traits/vector.hpp>

#include <gtest/gtest.h>

#include <cocaine/framework/service.hpp>

namespace cocaine {

namespace framework {

class dispatcher_t : public dispatch<cocaine::io::event_traits<io::locator::resolve>::upstream_type> {
    typedef dispatch<io::event_traits<io::locator::resolve>::upstream_type> super;

    loop_t& loop;

public:
    dispatcher_t(loop_t& loop) :
        super("resolve::dispatch"),
        loop(loop)
    {
        typedef io::protocol<io::event_traits<io::locator::resolve>::upstream_type>::scope protocol;

        using namespace std::placeholders;

        on<protocol::value>(std::bind(&dispatcher_t::on_resolve, this, _1, _2, _3));
        on<protocol::error>(std::bind(&dispatcher_t::on_error, this, _1, _2));
    }

private:
    void on_resolve(const std::vector<asio::ip::tcp::endpoint>& endpoints, unsigned int, const io::graph_basis_t&) {
        std::cout << "Value[endpoint='" << endpoints[0] << "']" << std::endl;
        loop.stop();
    }

    void on_error(int code, const std::string& reason) {
        std::cout << "Error: [" << code << "] " << reason << std::endl;
        loop.stop();
    }
};

} // namespace framework

} // namespace cocaine

using namespace cocaine::framework;

TEST(LowLevel, Locator) {
    asio::io_service loop;
    std::unique_ptr<asio::ip::tcp::socket> socket(new asio::ip::tcp::socket(loop));
    asio::ip::tcp::resolver r(loop);
    asio::ip::tcp::resolver::query q("localhost", "10053");
    asio::connect(*socket, r.resolve(q));

    auto dispatch = std::make_shared<dispatcher_t>(loop);

    cocaine::api::client<cocaine::io::locator_tag> client;
    client.connect(std::move(socket));
    client.invoke<cocaine::io::locator::resolve>(dispatch, std::string("node"));

    std::thread thread([&loop]{ loop.run(); });
    thread.join();
}
