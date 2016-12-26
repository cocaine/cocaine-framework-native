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

#include "cocaine/framework/detail/resolver.hpp"

#include <cocaine/idl/locator.hpp>
#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/error_code.hpp>
#include <cocaine/traits/graph.hpp>
#include <cocaine/traits/tuple.hpp>
#include <cocaine/traits/vector.hpp>

#include "cocaine/framework/scheduler.hpp"
#include "cocaine/framework/session.hpp"
#include "cocaine/framework/trace.hpp"

#include "cocaine/framework/detail/basic_session.hpp"
#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/detail/net.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;
using namespace cocaine::framework::detail;

namespace {

typedef std::tuple<std::vector<asio::ip::tcp::endpoint>, uint, io::graph_root_t> resolve_result;

resolver_t::result_t
on_resolve(task<resolve_result>::future_move_type future,
           std::shared_ptr<framework::session_t>,
           std::string name)
{
    try {
        auto result = future.get();
        CF_DBG("<< resolving - done");

        resolver_t::result_t res = {
            endpoints_cast<boost::asio::ip::tcp::endpoint>(std::get<0>(result)), std::get<1>(result)
        };

        return res;
    } catch (const response_error& err) {
        CF_DBG("<< resolving - resolve error: %s", err.what());
        if (err.id() == cocaine::error::locator_errors::service_not_available) {
            throw service_not_found(std::move(name));
        } else {
            throw;
        }
    } catch (const std::exception& err) {
        CF_DBG("<< resolving - resolve error: %s", err.what());
        throw;
    }
}

task<resolve_result>::future_type
on_invoke(task<channel<io::locator::resolve>>::future_move_type future, std::shared_ptr<framework::session_t>) {
    try {
        auto channel = future.get();
        return channel.rx.recv();
    } catch (const std::exception& err) {
        CF_DBG("<< resolving - invocation error: %s", err.what());
        throw;
    }
}

task<channel<io::locator::resolve>>::future_type
on_connect(task<void>::future_move_type future, std::shared_ptr<framework::session_t> locator, std::string name) {
    try {
        future.get();

        CF_DBG("<< connect to the locator: ok");
        CF_DBG(">> resolving ...");
        return locator->invoke<io::locator::resolve>(name);
    } catch (const std::system_error& err) {
        CF_DBG("<< connecting - error: %s", err.what());
        throw;
    }
}

} // namespace

resolver_t::resolver_t(scheduler_t& scheduler) :
    scheduler(scheduler)
{
    endpoints_.emplace_back(boost::asio::ip::tcp::v6(), 10053);
}

resolver_t::~resolver_t() {}

std::vector<resolver_t::endpoint_type> resolver_t::endpoints() const {
    return endpoints_;
}

void resolver_t::endpoints(std::vector<resolver_t::endpoint_type> endpoints) {
    endpoints_ = std::move(endpoints);
}

auto resolver_t::resolve(std::string name) -> task<resolver_t::result_t>::future_type {
    CF_CTX("R");

    auto locator = std::make_shared<framework::session_t>(scheduler);

    CF_DBG(">> connecting to the locator ...");
    return locator->connect(endpoints())
        .then(scheduler, trace::wrap(trace_t::bind(&on_connect, ph::_1, locator, name)))
        .then(scheduler, trace::wrap(trace_t::bind(&on_invoke, ph::_1, locator)))
        .then(scheduler, trace::wrap(trace_t::bind(&on_resolve, ph::_1, locator, name)));
}

serialized_resolver_t::serialized_resolver_t(std::vector<endpoint_type> endpoints, scheduler_t& scheduler) :
    resolver(scheduler),
    scheduler(scheduler)
{
    resolver.endpoints(std::move(endpoints));
}

auto serialized_resolver_t::resolve(std::string name) -> task<result_type>::future_type {
    std::unique_lock<std::mutex> lock(mutex);

    auto it = inprogress.find(name);
    if (it == inprogress.end()) {
        std::deque<task<result_type>::promise_type> queue;
        inprogress.insert(it, std::make_pair(name, queue));
        lock.unlock();
        return resolver.resolve(name)
            .then(scheduler, trace::wrap(trace_t::bind(&serialized_resolver_t::notify_all, shared_from_this(), ph::_1, name)));
    } else {
        task<result_type>::promise_type promise;
        auto future = promise.get_future();
        it->second.push_back(std::move(promise));
        return future;
    }
}

serialized_resolver_t::result_type
serialized_resolver_t::notify_all(task<result_type>::future_move_type future, std::string name) {
    std::lock_guard<std::mutex> lock(mutex);

    auto it = inprogress.find(name);
    if (it == inprogress.end()) {
        return future.get();
    }

    try {
        auto result = future.get();
        for (auto& promise : it->second) {
            promise.set_value(result);
        }
        inprogress.erase(it);
        return result;
    } catch (const std::system_error& err) {
        for (auto& promise : it->second) {
            promise.set_exception(err);
        }
        inprogress.erase(it);
        throw;
    }
}
