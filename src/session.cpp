/*
    Copyright (c) 2015 Evgeny Safronov <division494@gmail.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.
    This file is part of Cocaine.
    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/framework/session.hpp"

#include <asio/connect.hpp>

#include <cocaine/locked_ptr.hpp>

#include "cocaine/framework/scheduler.hpp"

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/detail/loop.hpp"
#include "cocaine/framework/detail/net.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

template<class BasicSession>
class session<BasicSession>::impl : public std::enable_shared_from_this<session<BasicSession>::impl> {
public:
    scheduler_t& scheduler;
    std::shared_ptr<basic_session_type> sess;
    synchronized<std::vector<endpoint_type>> endpoints;

    typedef std::vector<std::shared_ptr<task<void>::promise_type>> queue_type;
    synchronized<queue_type> queue;

    explicit impl(scheduler_t& scheduler) :
        scheduler(scheduler),
        sess(std::make_shared<basic_session_type>(scheduler))
    {}

    /// \warning call only from event loop thread, otherwise the behavior is undefined.
    void on_connect(task<std::error_code>::future_move_type future, std::shared_ptr<task<void>::promise_type> promise) {
        const auto ec = future.get();

        if (ec) {
            switch (ec.value()) {
            case asio::error::already_started:
                queue->push_back(promise);
                break;
            case asio::error::already_connected:
                promise->set_value();
                break;
            default:
                promise->set_exception(std::system_error(ec));
                queue.apply([&](queue_type& queue) {
                    for (auto it = queue.begin(); it != queue.end(); ++it) {
                        (*it)->set_exception(std::system_error(ec));
                    }
                    queue.clear();
                });
            }
        } else {
            promise->set_value();
            queue.apply([&](queue_type& queue) {
                for (auto it = queue.begin(); it != queue.end(); ++it) {
                    (*it)->set_value();
                }
                queue.clear();
            });
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
    d->sess->cancel();
}

template<class BasicSession>
bool session<BasicSession>::connected() const {
    return d->sess->connected();
}

template<class BasicSession>
auto session<BasicSession>::connect(const session::endpoint_type& endpoint) -> task<void>::future_type {
    return connect(std::vector<endpoint_type> {{ endpoint }});
}

template<class BasicSession>
auto session<BasicSession>::connect(const std::vector<session::endpoint_type>& endpoints) -> task<void>::future_type {
    if (endpoints == *d->endpoints.synchronize()) {
        return make_ready_future<void>::error(
            std::runtime_error("already in progress with different endpoint set")
        );
    }

    auto promise = std::make_shared<task<void>::promise_type>();
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
auto session<BasicSession>::invoke(std::function<io::encoder_t::message_type(std::uint64_t)> encoder)
    -> task<basic_invoke_result>::future_type
{
    return d->sess->invoke(std::move(encoder));
}

#include "cocaine/framework/detail/basic_session.hpp"
template class cocaine::framework::session<basic_session_t>;
