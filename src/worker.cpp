#include "cocaine/framework/worker.hpp"

#include <csignal>

#include <boost/thread/thread.hpp>

#include <asio/local/stream_protocol.hpp>
#include <asio/connect.hpp>

#include <cocaine/detail/service/node/messages.hpp>

#include "cocaine/framework/forwards.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

worker_t::worker_t(options_t options) :
    options(std::move(options))
{
    // TODO: Set default locator endpoint.
    // service_manager_t::endpoint_t locator_endpoint("127.0.0.1", 10053);

    // Block the deprecated signals.
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGPIPE);
    ::sigprocmask(SIG_BLOCK, &sigset, nullptr);
}

int worker_t::run() {
    session.reset(new worker_session_t(loop, dispatch));
    session->connect(options.endpoint, options.uuid);

    // The main thread is guaranteed to work only with cocaine socket and timers.
    // TODO: It may be a good idea to catch some typed exceptions, like disown_error etc and map
    //       it into an error code.
    loop.run();

    return 0;
}
