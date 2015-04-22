#include <cocaine/framework/service.hpp>
#include <cocaine/framework/manager.hpp>
#include <cocaine/idl/node.hpp>
#include <cocaine/framework/detail/log.hpp>
using namespace cocaine;
using namespace cocaine::framework;


namespace app {

typedef io::protocol<io::app::enqueue::dispatch_type>::scope scope;

task<boost::optional<std::string>>::future_type
on_send(task<channel<io::app::enqueue>::sender_type>::future_move_type future,
    channel<io::app::enqueue>::receiver_type rx)
{
    future.get();
    return rx.recv();
}

task<boost::optional<std::string>>::future_type
on_chunk(task<boost::optional<std::string>>::future_move_type future,
    channel<io::app::enqueue>::receiver_type rx)
{
    auto result = future.get();
    if (!result) {
        throw std::runtime_error("the `result` must be true");
    }
    return rx.recv();
}

void
on_choke(task<boost::optional<std::string>>::future_move_type future) {
    auto result = future.get();
}

task<void>::future_type
on_invoke(task<channel<io::app::enqueue>>::future_move_type future) {
    auto channel = future.get();
    auto tx = std::move(channel.tx);
    auto rx = std::move(channel.rx);
    return tx.send<scope::chunk>(std::string("le message"))
        .then(trace_t::bind(&on_send, std::placeholders::_1, rx))
        .then(trace_t::bind(&on_chunk, std::placeholders::_1, rx))
        .then(trace_t::bind(&on_choke, std::placeholders::_1));
}

void on_finalize(task<void>::future_move_type future) {
    future.get();
}
}


int main(int argc, char** argv) {

    service_manager_t manager(1), m_manager(1);
    uint iters        = 1;
    std::string app   = "echo-cpp";
    std::string event = "ping";


    auto echo = manager.create<cocaine::io::app_tag>("echo-cpp");
    trace_t trace = trace_t::generate("main", app);
    trace_t::restore_scope_t scope(trace);
    try {
        echo.connect().get();
        std::vector<task<void>::future_type> futures;
        futures.reserve(iters);

        for (uint id = 0; id < iters; ++id) {
            futures.emplace_back(
                    echo.invoke<cocaine::io::app::enqueue>(std::string(event))
                .then(trace_t::bind(&app::on_invoke, std::placeholders::_1))
                .then(trace_t::bind(&app::on_finalize, std::placeholders::_1))
            );
        }

        // Block here.
        for (auto& future : futures) {
            future.get();
        }
        CF_DBG("ss");
        sleep(1);
    }
    catch(...) {
        CF_DBG("ss");
        sleep(1);
    }
}
