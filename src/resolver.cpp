#include "cocaine/framework/detail/resolver.hpp"

#include <cocaine/idl/locator.hpp>
#include <cocaine/traits/graph.hpp>
#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/tuple.hpp>
#include <cocaine/traits/vector.hpp>

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/scheduler.hpp"
#include "cocaine/framework/session.hpp"
#include "cocaine/framework/util/net.hpp"

#include "cocaine/framework/detail/basic_session.hpp"

using namespace cocaine::framework::detail;

resolver_t::resolver_t(scheduler_t& scheduler) :
    scheduler_(scheduler)
{
    endpoints_.emplace_back(boost::asio::ip::tcp::v6(), 10053);
}

resolver_t::~resolver_t() {}

void resolver_t::timeout(std::chrono::milliseconds) {
    // Not implemented yet.
    COCAINE_ASSERT(false);
}

void resolver_t::endpoints(std::vector<resolver_t::endpoint_type>) {
    // Not implemented yet.
    COCAINE_ASSERT(false);
}

struct completer_t {
    void operator()() {
        CF_DBG("connecting to the locator ...");
    }
};

auto resolver_t::resolve(std::string name) -> typename task<resolver_result_t>::future_type {
    CF_CTX("R");

    session_t locator(scheduler_);
    try {
        locator.connect(endpoints_).get();
    } catch (const std::exception &err) {
        CF_DBG("connecting - error: %s", err.what());
        return make_ready_future<resolver_result_t>::error(err);
    }

    CF_DBG("resolving ...");
    try {
        auto ch = locator.invoke<io::locator::resolve>(name).get();
        auto rx = std::move(std::get<1>(ch));
        auto result = rx.recv().get();
        CF_DBG("resolving - done");

        resolver_result_t res = {
            util::endpoints_cast<boost::asio::ip::tcp::endpoint>(std::get<0>(result)), std::get<1>(result)
        };
        return make_ready_future<resolver_result_t>::value(res);
    } catch (const std::exception &err) {
        CF_DBG("resolving - error: %s", err.what());
        return make_ready_future<resolver_result_t>::error(err);
    }
}
