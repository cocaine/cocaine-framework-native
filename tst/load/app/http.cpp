#include <gtest/gtest.h>

#include <cocaine/idl/node.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/manager.hpp>

#include "../config.hpp"
#include "../stats.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

using namespace testing;
using namespace testing::load;

typedef io::protocol<io::app::enqueue::dispatch_type>::scope scope;

namespace testing { namespace load { namespace app { namespace http {

task<boost::optional<std::string>>::future_type
on_send(task<channel<io::app::enqueue>::sender_type>::future_move_type future,
        channel<io::app::enqueue>::receiver_type rx)
{
    future.get();
    return rx.recv();
}

task<boost::optional<std::string>>::future_type
on_headers(task<boost::optional<std::string>>::future_move_type future,
           channel<io::app::enqueue>::receiver_type rx)
{
    auto payload = future.get();

    int code;
    std::vector<std::pair<std::string, std::string>> headers;

    msgpack::unpacked msg;
    msgpack::unpack(&msg, payload->data(), payload->size());
    std::tuple<int, decltype(headers)> obj;
    io::type_traits<decltype(obj)>::unpack(msg.get(), obj);

    std::tie(code, headers) = obj;
    EXPECT_EQ(200, code);

    return rx.recv();
}

task<boost::optional<std::string>>::future_type
on_body(task<boost::optional<std::string>>::future_move_type future,
        channel<io::app::enqueue>::receiver_type rx)
{
    auto payload = future.get();

    msgpack::unpacked msg;
    msgpack::unpack(&msg, payload->data(), payload->size());
    std::string obj;
    cocaine::io::type_traits<std::string>::unpack(msg.get(), obj);

    return rx.recv();
}

void
on_close(task<boost::optional<std::string>>::future_move_type future) {
    auto result = future.get();
    EXPECT_FALSE(result);
}

task<void>::future_type
on_invoke(task<channel<io::app::enqueue>>::future_move_type future) {
    auto channel = future.get();
    auto tx = std::move(channel.tx);
    auto rx = std::move(channel.rx);

    std::vector<std::pair<std::string, std::string>> headers;

    msgpack::sbuffer buf;
    msgpack::pack(buf, std::make_tuple(std::string("GET"), std::string("/"), std::string("1.1"), headers, std::string("")));

    return tx.send<scope::chunk>(std::string(buf.data(), buf.size()))
        .then(std::bind(&on_send, ph::_1, rx))
        .then(std::bind(&on_headers, ph::_1, rx))
        .then(std::bind(&on_body, ph::_1, rx))
        .then(std::bind(&on_close, ph::_1));
}

} } } } // namespace testing::load::app::http

TEST(load, http) {
    uint iters        = 0;
    std::string app   = "http";
    std::string event = "http";
    load_config("load.app.http", iters, app, event);

    std::atomic<int> counter(0);

    stats_guard_t stats(iters);

    service_manager_t manager;
    auto http = manager.create<io::app_tag>(app);
    http.connect().get();

    std::vector<task<void>::future_type> futures;
    futures.reserve(iters);

    for (uint id = 0; id < iters; ++id) {
        load_context context(id, counter, stats.stats);

        futures.emplace_back(
            http.invoke<io::app::enqueue>(event)
                .then(std::bind(&app::http::on_invoke, ph::_1))
                .then(std::bind(&finalize, ph::_1, std::move(context)))
        );
    }

    // Block here.
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(iters, counter);
}

