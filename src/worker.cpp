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

#include "cocaine/framework/worker.hpp"

#include <csignal>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>

#include <asio/local/stream_protocol.hpp>
#include <asio/connect.hpp>

#include "cocaine/framework/error.hpp"
#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/manager.hpp"
#include "cocaine/framework/scheduler.hpp"

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/detail/loop.hpp"
#include "cocaine/framework/detail/worker/executor.hpp"
#include "cocaine/framework/detail/worker/session.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;
using namespace cocaine::framework::detail;

namespace {

static inline
std::tuple<std::string, std::string>
parse_endpoint(const std::string& endpoint) {
    const auto pos = endpoint.rfind(':');
    if (pos == std::string::npos) {
        return std::make_tuple(endpoint, "10053");
    }

    return std::make_tuple(endpoint.substr(0, pos), endpoint.substr(pos + 1));
}

} // namespace

class worker_t::impl {
public:
    /// Control event loop.
    detail::loop_t io;
    event_loop_t loop;
    scheduler_t scheduler;

    options_t options;
    dispatch_type dispatch;

    /// Userland executor.
    detail::worker::executor_t executor;

    /// Service manager, for user purposes.
    service_manager_t manager;

    std::shared_ptr<worker_session_t> session;

    impl(options_t options) :
        loop(io),
        scheduler(loop),
        options(std::move(options)),
        executor(),
        manager(1)
    {}
};

worker_t::worker_t(options_t options) :
    d(new impl(std::move(options)))
{
    CF_DBG("initializing '%s' worker ...", d->options.name.c_str());

    std::vector<std::string> splitted;
    boost::algorithm::split(splitted, d->options.locator, boost::algorithm::is_any_of(","));

    CF_DBG("parsing locator endpoints from '%s' ...", d->options.locator.c_str());

    std::vector<session_t::endpoint_type> endpoints;
    std::transform(splitted.begin(), splitted.end(), std::back_inserter(endpoints), [](const std::string& endpoint) {
        std::string address;
        std::string port;
        std::tie(address, port) = parse_endpoint(endpoint);
        return boost::asio::ip::tcp::endpoint {
            boost::asio::ip::address::from_string(address),
            boost::lexical_cast<std::uint16_t>(port)
        };
    });

    CF_DBG("locator endpoints (%lu total):", endpoints.size());
    for (__attribute__((unused)) const auto& endpoint : endpoints) {
        CF_DBG(" - %s", CF_MSG(endpoint).c_str());
    }

    d->manager.endpoints(std::move(endpoints));

    // Block the deprecated signals.
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGPIPE);
    ::sigprocmask(SIG_BLOCK, &sigset, nullptr);
}

worker_t::~worker_t() {}

service_manager_t& worker_t::manager() {
    return d->manager;
}

void worker_t::on(std::string event, handler_type handler) {
    d->dispatch.on(event, std::move(handler));
}

auto worker_t::options() const -> const options_t& {
    return d->options;
}

int worker_t::run() {
    auto executor = std::bind(&detail::worker::executor_t::operator(), std::ref(d->executor), ph::_1);
    d->session.reset(new worker_session_t(d->dispatch, d->scheduler, executor));
    d->session->connect(d->options.endpoint, d->options.uuid);

    // The main thread is guaranteed to work only with cocaine socket and timers.
    try {
        d->loop.loop.run();
    } catch (const error_t& err) {
        return err.code().value();
    }

    return 0;
}
