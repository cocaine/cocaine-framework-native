#include "cocaine/framework/detail/resolver.hpp"

#include <cocaine/idl/locator.hpp>
#include <cocaine/traits/graph.hpp>
#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/tuple.hpp>
#include <cocaine/traits/vector.hpp>

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/scheduler.hpp"
#include "cocaine/framework/session.hpp"

#include "cocaine/framework/detail/basic_session.hpp"
#include "cocaine/framework/detail/net.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;
using namespace cocaine::framework::detail;

namespace {

typedef std::tuple<std::vector<asio::ip::tcp::endpoint>, uint, io::graph_basis_t> resolve_result;

resolver_t::result_t
on_resolve(typename task<resolve_result>::future_move_type future, std::shared_ptr<session_t>) {
    try {
        auto result = future.get();
        CF_DBG("<< resolving - done");

        resolver_t::result_t res = {
            endpoints_cast<boost::asio::ip::tcp::endpoint>(std::get<0>(result)), std::get<1>(result)
        };

        return res;
    } catch (const framework::error_t& err) {
        CF_DBG("<< resolving - resolve error: [%d] %s", err.id, err.reason.c_str());
        if (err.id == error::locator_errors::service_not_available) {
            throw service_not_found_error();
        } else {
            throw;
        }
    } catch (const std::exception& err) {
        CF_DBG("<< resolving - resolve error: %s", err.what());
        throw;
    }
}

typename task<resolve_result>::future_type
on_invoke(typename task<typename session_t::invoke_result<io::locator::resolve>::type>::future_move_type future,
          std::shared_ptr<session_t>)
{
    try {
        auto ch = future.get();
        auto rx = std::move(std::get<1>(ch));
        return rx.recv();
    } catch (const std::exception& err) {
        CF_DBG("<< resolving - invocation error: %s", err.what());
        throw;
    }
}

task<typename session_t::invoke_result<io::locator::resolve>::type>::future_type
on_connect(typename task<void>::future_move_type future,
           std::shared_ptr<session_t> locator,
           std::string name)
{
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

void resolver_t::timeout(std::chrono::milliseconds) {
    throw std::runtime_error("resolver_t::timeout: not implemented yet");
}

std::vector<resolver_t::endpoint_type> resolver_t::endpoints() const {
    return endpoints_;
}

void resolver_t::endpoints(std::vector<resolver_t::endpoint_type>) {
    throw std::runtime_error("resolver_t::endpoints: not implemented yet");
}

auto resolver_t::resolve(std::string name) -> typename task<resolver_t::result_t>::future_type {
    CF_CTX("R");

    auto locator = std::make_shared<session_t>(scheduler);

    CF_DBG(">> connecting to the locator ...");
    return locator->connect(endpoints())
        .then(scheduler, wrap(std::bind(&on_connect, ph::_1, locator, std::move(name))))
        .then(scheduler, wrap(std::bind(&on_invoke, ph::_1, locator)))
        .then(scheduler, wrap(std::bind(&on_resolve, ph::_1, locator)));
}


serialized_resolver_t::serialized_resolver_t(scheduler_t& scheduler) :
    resolver(scheduler),
    scheduler(scheduler)
{}

auto serialized_resolver_t::resolve(std::string name) -> typename task<result_type>::future_type {
    std::lock_guard<std::mutex> lock(mutex);

    auto it = inprogress.find(name);
    if (it == inprogress.end()) {
        std::deque<typename task<result_type>::promise_type> queue;
        inprogress.insert(it, std::make_pair(name, queue));
        // Use scheduler to avoid deadlock.
        return resolver.resolve(name)
            .then(scheduler, std::bind(&serialized_resolver_t::notify_all, shared_from_this(), ph::_1, name));
    } else {
        typename task<result_type>::promise_type promise;
        auto future = promise.get_future();
        it->second.push_back(std::move(promise));
        return future;
    }
}

serialized_resolver_t::result_type
serialized_resolver_t::notify_all(typename task<result_type>::future_move_type future, std::string name) {
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
