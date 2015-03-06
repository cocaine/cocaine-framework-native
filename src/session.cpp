#include "cocaine/framework/session.hpp"

#include <asio/connect.hpp>

#include <cocaine/locked_ptr.hpp>

#include "cocaine/framework/scheduler.hpp"
#include "cocaine/framework/util/net.hpp"

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/detail/loop.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

template<class BasicSession>
class session<BasicSession>::impl : public std::enable_shared_from_this<session<BasicSession>::impl> {
public:
    scheduler_t& scheduler;
    std::shared_ptr<basic_session_type> sess;
    synchronized<std::vector<endpoint_type>> endpoints;
    std::vector<std::shared_ptr<typename task<void>::promise_type>> queue;

    explicit impl(scheduler_t& scheduler) :
        scheduler(scheduler),
        sess(std::make_shared<basic_session_type>(scheduler))
    {}

    /// \warning call only from event loop thread, otherwise the behavior is undefined.
    void on_connect(typename task<std::error_code>::future_type& f, std::shared_ptr<typename task<void>::promise_type> promise) {
        const auto ec = f.get();

        if (ec) {
            switch (ec.value()) {
            case asio::error::already_started:
                queue.push_back(promise);
                break;
            case asio::error::already_connected:
                COCAINE_ASSERT(queue.empty());
                promise->set_value();
                break;
            default:
                promise->set_exception(std::system_error(ec));
                for (auto it = queue.begin(); it != queue.end(); ++it) {
                    (*it)->set_exception(std::system_error(ec));
                }
                queue.clear();
            }
        } else {
            promise->set_value();
            for (auto it = queue.begin(); it != queue.end(); ++it) {
                (*it)->set_value();
            }
            queue.clear();
        }
    }
};

template<class BasicSession>
session<BasicSession>::session(scheduler_t& scheduler) :
    d(new impl(scheduler)),
    scheduler(scheduler)
{}

template<class BasicSession>
session<BasicSession>::~session() {
    d->sess->disconnect();
}

template<class BasicSession>
bool session<BasicSession>::connected() const {
    return d->sess->connected();
}

template<class BasicSession>
auto session<BasicSession>::connect(const session::endpoint_type& endpoint) -> typename task<void>::future_type {
    return connect(std::vector<endpoint_type> {{ endpoint }});
}

template<class BasicSession>
auto session<BasicSession>::connect(const std::vector<session::endpoint_type>& endpoints) -> typename task<void>::future_type {
    if (endpoints == *d->endpoints.synchronize()) {
        return make_ready_future<void>::error(
            std::runtime_error("already in progress with different endpoint set")
        );
    }

    auto promise = std::make_shared<typename task<void>::promise_type>();
    auto future = promise->get_future();

    d->sess->connect(endpoints)
        .then(d->scheduler, wrap(std::bind(&impl::on_connect, d, ph::_1, promise)));

    return future;
}

template<class BasicSession>
auto session<BasicSession>::endpoint() const -> boost::optional<endpoint_type> {
    return d->sess->endpoint();
}

template<class BasicSession>
auto session<BasicSession>::next() -> std::uint64_t {
    return d->sess->next();
}

template<class BasicSession>
auto session<BasicSession>::invoke(std::uint64_t span, io::encoder_t::message_type&& message) -> typename task<basic_invoke_result>::future_type {
    return d->sess->invoke(span, std::move(message));
}

#include "cocaine/framework/detail/basic_session.hpp"
template class cocaine::framework::session<basic_session_t>;
