#include <boost/thread/thread.hpp>

#include <gtest/gtest.h>

#include <cocaine/idl/node.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/manager.hpp>

using namespace cocaine;
using namespace cocaine::framework;

typedef typename io::protocol<io::app::enqueue::dispatch_type>::scope scope;

namespace {

struct inline_executor {
    void operator()(std::function<void()> closure) {
        closure();
    }
};

} // namespace

static const inline_executor executor = inline_executor();

TEST(load, EchoSyncST) {
    service_manager_t manager(1);
    auto echo = manager.create<cocaine::io::app_tag>("echo-cpp");

    for (size_t id = 0; id < 10000; ++id) {
        auto channel = echo.invoke<io::app::enqueue>(std::string("ping")).get();
        auto tx = std::move(std::get<0>(channel));
        auto rx = std::move(std::get<1>(channel));

        tx.send<scope::chunk>(std::string("le message")).get();

        auto result = rx.recv().get();
        EXPECT_EQ("le message", *result);

        auto choke = rx.recv().get();
        EXPECT_FALSE(choke);
    }
}

namespace ph = std::placeholders;

namespace {

typename task<boost::optional<std::string>>::future_type
on_send(typename task<sender<io::app::enqueue::dispatch_type, basic_session_t>>::future_move_type future,
        receiver<io::app::enqueue::upstream_type, basic_session_t> rx)
{
    future.get();
    return rx.recv();
}

typename task<boost::optional<std::string>>::future_type
on_chunk(typename task<boost::optional<std::string>>::future_move_type future,
        receiver<io::app::enqueue::upstream_type, basic_session_t> rx)
{
    auto result = future.get();
    EXPECT_EQ("le message", *result);

    return rx.recv();
}

void
on_choke(typename task<boost::optional<std::string>>::future_move_type future,
         std::atomic<int>& counter)
{
    auto result = future.get();
    EXPECT_FALSE(result);

    counter++;
}

typename task<void>::future_type
on_invoke(typename task<typename invocation_result<io::app::enqueue>::type>::future_move_type future,
          std::atomic<int>& counter)
{
    auto channel = future.get();
    auto tx = std::move(std::get<0>(channel));
    auto rx = std::move(std::get<1>(channel));
    return tx.send<scope::chunk>(std::string("le message"))
        .then(std::bind(&on_send, ph::_1, rx))
        .then(std::bind(&on_chunk, ph::_1, rx))
        .then(std::bind(&on_choke, ph::_1, std::ref(counter)));
}

} // namespace

TEST(load, EchoAsyncST) {
    const int ITERS = 1000;

    std::atomic<int> counter(0);

    service_manager_t manager(1);
    auto echo = manager.create<cocaine::io::app_tag>("echo-cpp");

    std::vector<typename task<void>::future_type> futures;
    futures.reserve(ITERS);

    for (int i = 0; i < ITERS; ++i) {
        futures.emplace_back(
            echo.invoke<io::app::enqueue>(std::string("ping"))
                .then(std::bind(&on_invoke, ph::_1, std::ref(counter)))
        );
    }

    // Block here.
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(ITERS, counter);
}

TEST(basic_service_t, EchoMT) {
//    typedef typename io::protocol<io::app::enqueue::dispatch_type>::scope upstream;

//    service_manager_t manager(1);

//    std::vector<boost::thread> threads;

//    for (size_t tid = 0; tid < 8; ++tid) {
//        threads.emplace_back(boost::thread([tid, &manager]{
//            auto echo = manager.create<cocaine::io::app_tag>("echo-cpp");

//            for (size_t id = 0; id < 10000; ++id) {
//                auto ch = echo->invoke<cocaine::io::app::enqueue>(std::string("ping")).get();
//                auto tx = std::move(std::get<0>(ch));
//                auto rx = std::move(std::get<1>(ch));
//                auto chunk = std::to_string(tid) + "/" + std::to_string(id) + ": le message";
//                tx.send<upstream::chunk>(chunk).get();
//                auto result = rx.recv().get();
//                rx.recv().get();

//                EXPECT_EQ(chunk, *result);
//                if (chunk != *result) {
//                    std::terminate();
//                }
//            }
//        }));
//    }

//    for (auto& thread : threads) {
//        thread.join();
//    }
}
