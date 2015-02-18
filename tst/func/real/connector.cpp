#include <boost/asio/ip/tcp.hpp>

#include <gtest/gtest.h>

#include <cocaine/framework/forwards.hpp>
#include <cocaine/framework/detail/log.hpp>
#include <cocaine/framework/resolver.hpp>

#include <cocaine/framework/scheduler.hpp>
#include <cocaine/framework/detail/loop.hpp>

#include "../../util/net.hpp"

using namespace testing;
using namespace testing::util;

using namespace cocaine::framework;

TEST(Resolver, Resolve) {
    client_t client;
    event_loop_t loop { client.loop() };
    scheduler_t scheduler(loop);

    resolver_t resolver(scheduler);
    auto result = resolver.resolve("echo").get();

    EXPECT_FALSE(result.endpoints.empty());
    EXPECT_EQ(1, result.version);
}

//class connector_t {
//public:
//    typedef resolver_t::endpoint_type endpoint_type;

//public:
//    // Queue.
//    connector_t(resolver_t& resolver, loop_t&);
//    // connector_t(resolver_t*, scheduler_t*);
//    ~connector_t();

//    void timeout(std::chrono::duration);
//    void endpoints(std::vector<endpoint_type>);

//    auto connect(std::string) -> future_type<std::shared_ptr<basic_session_t>>;
//};

//TEST(Connector, SingleConnect) {
//    loop.run(); // in thread

//    cocaine::framework::connector connector(loop);
//    connector.connect("node").get();
//}

//TEST(Connector, MultipleConnect) {
//    cocaine::framework::connector connector(loop);
//    connector.connect("node").then();
//    connector.connect("node").then();

//    loop.run();
//}

//TEST(Connector, SequentialConnect) {
//    loop.run(); // in thread

//    cocaine::framework::connector connector(loop);
//    connector.connect("node").get();
//    // Check
//    connector.connect("node").get(); // Already
//}

//TEST(Connector, Abort) {
//    {
//        cocaine::framework::connector connector(loop);
//        connector.connect("node").then(); // operation_aborted
//    }

//    loop.run();
//}
