#include <gtest/gtest.h>

#include <cocaine/idl/node.hpp>
#include <cocaine/rpc/upstream.hpp>

#include <cocaine/framework/connection.hpp>

using namespace cocaine::framework;

template<class T>
class sender_t {};

template<class Event>
class receiver_t {
public:
    template<typename T>
    future_t<T> recv() {
    }
};

template<class T, class E>
struct channel_t {
    sender_t<E>   tx;
    receiver_t<E> rx;
};

template<class T>
class service_t {

    template<typename Event>
    using upstream_t = cocaine::upstream<typename cocaine::io::event_traits<Event>::dispatch_type>;

    std::string name;
    std::shared_ptr<connection_t> connection;

public:
    service_t(std::string name, std::shared_ptr<connection_t> connection) :
        name(std::move(name)),
        connection(connection)
    {}

    ~service_t() {
        // TODO: Block if not detached.
    }

    future_t<void> connect() {
//        auto resolver = std::make_shared<connection_t>(connection.io());
//        return resolver->connect(io::ip::tcp::endpoint(io::ip::tcp::v4(), 10053))
//            .then([this, resolver](future_t<void> f){
//            // Again, need mutex and queue to prevent multiple connection.
//            f.get(); // May throw if not connected.
//            resolver->invoke<locator::resolve>(name).then(f -> connection->connect(endpoint));
//        });

//        return connection->connect(endpoint);
    }

    void detach() {}

    // TODO: Maybe return future, because if the service isn't connected it takes time to auto reconnect.
    template<class Event, typename... Args>
    channel_t<T, Event> invoke(Args&&... args) {
        // 1. Check connection.
        // 1.1. If not connected - connect and get().
        // 1.2. If connected - okay.
        // 2. Create token, pack and send message.
    }
};

//TEST(Functional, Node) {
//    loop_t loop;
//    std::thread thread([&loop]{
//        loop_t::work work(loop);
//        loop.run();
//    });

//    service_t<cocaine::io::node> service("node", std::make_shared<connection_t>(loop));
//    service.connect().get();
//    auto chan = service.invoke<cocaine::io::node::list>();
//    auto list = chan.rx.recv<cocaine::dynamic_t>().get(); // protocol::value?
//    EXPECT_EQ(cocaine::dynamic_t(std::vector<std::string>({ "echo", "rails" })), list);

//    loop.stop();
//    thread.join();
//}
