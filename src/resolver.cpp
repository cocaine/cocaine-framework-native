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
    } catch (const cocaine_error& err) {
        CF_DBG("<< resolving - resolve error: [%d] %s", err.id, err.reason.c_str());
        if (err.id == error::locator_errors::service_not_available) {
            throw service_not_found_error();
        } else {
            throw err;
        }
    } catch (const std::exception& err) {
        CF_DBG("<< resolving - resolve error: %s", err.what());
        throw err;
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
    } catch (const std::exception &err) {
        CF_DBG("<< resolving - invocation error: %s", err.what());
        throw err;
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
        throw err;
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
