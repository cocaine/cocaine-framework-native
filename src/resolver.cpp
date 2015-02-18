#include "cocaine/framework/resolver.hpp"

#include <cocaine/idl/locator.hpp>
#include <cocaine/traits/graph.hpp>
#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/tuple.hpp>
#include <cocaine/traits/vector.hpp>

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/scheduler.hpp"
#include "cocaine/framework/util/net.hpp"

#include "cocaine/framework/session.hpp"

using namespace cocaine::framework;

class resolver_t::impl {
public:
    scheduler_t& scheduler;
    std::vector<resolver_t::endpoint_type> endpoints;

    impl(scheduler_t& scheduler) : scheduler(scheduler) {}
};

resolver_t::resolver_t(scheduler_t& scheduler) :
    d(new impl(scheduler))
{
    d->endpoints.emplace_back(boost::asio::ip::tcp::v6(), 10053);
}

resolver_t::~resolver_t() {}

void resolver_t::timeout(std::chrono::milliseconds) {

}

void resolver_t::endpoints(std::vector<resolver_t::endpoint_type> endpoints) {

}

namespace {

void on_locator_connect(future_type<void>&) {

}

}

struct completer_t {
    void operator()() {
        CF_DBG("connecting to the locator ...");
    }
};

auto resolver_t::resolve(std::string name) -> future_type<resolver_result_t> {
    CF_CTX("resolver");
    CF_CTX("resoling '%s'", name);
    CF_DBG("connecting to the locator ...");

    auto locator = std::make_shared<session<io::locator_tag>>(std::make_shared<basic_session_t>(d->scheduler));
    // TODO: Support more than 1 endpoint.
//    locator->connect(d->endpoints[0]).then(d->scheduler, completer);
    try {
        locator->connect(d->endpoints[0]).get();
    } catch (const std::exception &err) {
        CF_DBG("connecting - error: %s", err.what());
        return make_ready_future<resolver_result_t>::error(err);
    }

    CF_DBG("resolving ...");
    try {
        auto ch = locator->template invoke<io::locator::resolve>(name).get();
        auto rx = std::move(std::get<1>(ch));
        auto result = rx.recv().get();
        CF_DBG("resolving - done");
        locator->disconnect();

        resolver_result_t res = { util::endpoints_cast<boost::asio::ip::tcp::endpoint>(std::get<0>(result)), std::get<1>(result) };
        return make_ready_future<resolver_result_t>::value(res);
    } catch (const std::exception &err) {
        CF_DBG("resolving - error: %s", err.what());
        return make_ready_future<resolver_result_t>::error(err);
    }
}
