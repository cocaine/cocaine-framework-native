#include <gtest/gtest.h>

#include <fstream>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/density.hpp>
#include <boost/thread.hpp>

#include <cocaine/idl/node.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/manager.hpp>
#include <cocaine/framework/detail/log.hpp>

#include "../util/env.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

typedef io::protocol<io::app::enqueue::dispatch_type>::scope scope;

typedef boost::accumulators::accumulator_set<
    double,
    boost::accumulators::stats<
        boost::accumulators::tag::tail_quantile<boost::accumulators::right>
    >
> acc_t;

namespace ab {

task<boost::optional<std::string>>::future_type
on_send(task<sender<io::app::enqueue::dispatch_type, basic_session_t>>::future_move_type future,
        receiver<io::app::enqueue::upstream_type, basic_session_t> rx)
{
    future.get();
    return rx.recv();
}

task<boost::optional<std::string>>::future_type
on_chunk(task<boost::optional<std::string>>::future_move_type future,
        receiver<io::app::enqueue::upstream_type, basic_session_t> rx)
{
    auto result = future.get();
    if (!result) {
        throw std::runtime_error("`result` must be true");
    }
    EXPECT_EQ("le message", *result);

    return rx.recv();
}

void
on_choke(task<boost::optional<std::string>>::future_move_type future, std::atomic<int>& counter) {
    auto result = future.get();
    EXPECT_FALSE(result);

    counter++;
}

task<void>::future_type
on_invoke(task<invocation_result<io::app::enqueue>::type>::future_move_type future,
          std::atomic<int>& counter)
{
    auto channel = future.get();
    auto tx = std::move(channel.tx);
    auto rx = std::move(channel.rx);
    return tx.send<scope::chunk>(std::string("le message"))
        .then(std::bind(&on_send, ph::_1, rx))
        .then(std::bind(&on_chunk, ph::_1, rx))
        .then(std::bind(&on_choke, ph::_1, std::ref(counter)));
}

void on_end(task<void>::future_move_type future,
            std::chrono::high_resolution_clock::time_point start,
            uint i,
            acc_t& acc)
{
    auto now = std::chrono::high_resolution_clock::now();

    auto elapsed = std::chrono::duration<float, std::chrono::milliseconds::period>(now - start).count();
    acc(elapsed);
    future.get();
    CF_DBG("<<< %d.", i + 1);
}

namespace http {

task<boost::optional<std::string>>::future_type
on_send(task<sender<io::app::enqueue::dispatch_type, basic_session_t>>::future_move_type future,
        receiver<io::app::enqueue::upstream_type, basic_session_t> rx)
{
    future.get();
    return rx.recv();
}

task<boost::optional<std::string>>::future_type
on_headers(task<boost::optional<std::string>>::future_move_type future,
           receiver<io::app::enqueue::upstream_type, basic_session_t> rx)
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
        receiver<io::app::enqueue::upstream_type, basic_session_t> rx)
{
    auto payload = future.get();

    msgpack::unpacked msg;
    msgpack::unpack(&msg, payload->data(), payload->size());
    std::string obj;
    cocaine::io::type_traits<std::string>::unpack(msg.get(), obj);

    return rx.recv();
}

void
on_close(task<boost::optional<std::string>>::future_move_type future, std::atomic<int>& counter) {
    auto result = future.get();
    EXPECT_FALSE(result);

    counter++;
}

task<void>::future_type
on_invoke(task<invocation_result<io::app::enqueue>::type>::future_move_type future,
          std::atomic<int>& counter)
{
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
        .then(std::bind(&on_close, ph::_1, std::ref(counter)));
}

} // namespace http

} // namespace ab
#include <cocaine/framework/detail/loop.hpp>
TEST(load, ab) {
    const std::string CONFIG_FILENAME = testing::util::get_option<std::string>("CF_CFG", "");

    std::string app = "echo-cpp";
    std::string event = "ping";
    uint iters = 100;

    if (!CONFIG_FILENAME.empty()) {
        std::ifstream infile(CONFIG_FILENAME);
        infile >> app >> event >> iters;
    }

    acc_t acc(boost::accumulators::tag::tail<boost::accumulators::right>::cache_size = iters);

    std::atomic<int> counter(0);

    {
        service_manager_t manager(1);
        auto echo = manager.create<cocaine::io::app_tag>(app);
        echo.connect().get();

        std::vector<task<void>::future_type> futures;
        futures.reserve(iters);

        for (uint i = 0; i < iters; ++i) {
            auto now = std::chrono::high_resolution_clock::now();
            CF_DBG(">>> %d.", i + 1);
            futures.emplace_back(
                echo.invoke<io::app::enqueue>(std::string(event))
                    .then(std::bind(&ab::on_invoke, ph::_1, std::ref(counter)))
                    .then(std::bind(&ab::on_end, ph::_1, now, i, std::ref(acc)))
            );
        }

        // Block here.
        for (auto& future : futures) {
            future.get();
        }
    }

    EXPECT_EQ(iters, counter);

    for (auto probability : { 0.50, 0.75, 0.90, 0.95, 0.98, 0.99, 0.9995 }) {
        const double val = boost::accumulators::quantile(
            acc,
            boost::accumulators::quantile_probability = probability
        );

        fprintf(stdout, "%6.2f%% : %6.3fms\n", 100 * probability, val);
    }
}

TEST(load, DISABLE_ab_http) {
    const std::string CONFIG_FILENAME = testing::util::get_option<std::string>("CF_CFG", "");

    std::string app = "nodejs";
    std::string event = "http";
    uint iters = 100;

    if (!CONFIG_FILENAME.empty()) {
        std::ifstream infile(CONFIG_FILENAME);
        infile >> app >> event >> iters;
    }

    acc_t acc(boost::accumulators::tag::tail<boost::accumulators::right>::cache_size = iters);

    std::atomic<int> counter(0);

    service_manager_t manager(1);
    auto echo = manager.create<cocaine::io::app_tag>(app);
    echo.connect().get();

    std::vector<task<void>::future_type> futures;
    futures.reserve(iters);

    for (uint i = 0; i < iters; ++i) {
        auto now = std::chrono::high_resolution_clock::now();
        CF_DBG(">>> %d.", i + 1);
        futures.emplace_back(
            echo.invoke<io::app::enqueue>(event)
                .then(std::bind(&ab::http::on_invoke, ph::_1, std::ref(counter)))
                .then(std::bind(&ab::on_end, ph::_1, now, i, std::ref(acc)))
        );
    }

    // Block here.
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(iters, counter);

    for (auto probability : { 0.50, 0.75, 0.90, 0.95, 0.98, 0.99, 0.9995 }) {
        const double val = boost::accumulators::quantile(
            acc,
            boost::accumulators::quantile_probability = probability
        );

        fprintf(stdout, "%6.2f%% : %6.3fms\n", 100 * probability, val);
    }
}


// Запись в сокет без отложений реально помогает сократить тайминги в два раза!
// Отложенные disconnect, revoke, кажется, не особо влияют.
// Две очереди событий?
// priority loop?
// Несколько тредов на один евент луп.
// jemalloc?
// boost::future?
