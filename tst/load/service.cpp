#include <boost/thread/thread.hpp>

#include <gtest/gtest.h>

#include <cocaine/idl/node.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/manager.hpp>

using namespace cocaine;
using namespace cocaine::framework;

TEST(basic_service_t, EchoMT) {
    typedef typename io::protocol<io::app::enqueue::dispatch_type>::scope upstream;

    service_manager_t manager(1);

    std::vector<boost::thread> threads;

    for (size_t tid = 0; tid < 8; ++tid) {
        threads.emplace_back(boost::thread([tid, &manager]{
            auto echo = manager.create<cocaine::io::app_tag>("echo-cpp");

            for (size_t id = 0; id < 10000; ++id) {
                auto ch = echo->invoke<cocaine::io::app::enqueue>(std::string("ping")).get();
                auto tx = std::move(std::get<0>(ch));
                auto rx = std::move(std::get<1>(ch));
                auto chunk = std::to_string(tid) + "/" + std::to_string(id) + ": le message";
                tx.send<upstream::chunk>(chunk).get();
                auto result = rx.recv().get();
                rx.recv().get();

                EXPECT_EQ(chunk, *result);
                if (chunk != *result) {
                    std::terminate();
                }
            }
        }));
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

TEST(service, EchoAsyncST) {
//    client_t client;
//    event_loop_t loop { client.loop() };
//    scheduler_t scheduler(loop);
//    basic_service_t echo("echo-cpp", 1, scheduler);

//    std::vector<typename task<void>::future_type> futures;
//    for (int i = 0; i < 10000; ++i) {
//        futures.emplace_back(
//            echo.invoke<cocaine::io::app::enqueue>(std::string("ping"))
//                .then(scheduler, std::bind(&on_invoke, ph::_1, std::ref(scheduler)))
//        );
//    }

//    // Block here.
//    for (auto& future : futures) {
//        future.get();
//    }
}
