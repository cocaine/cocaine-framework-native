#include "cocaine/framework/worker.hpp"

#include <csignal>

#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>

#include <asio/local/stream_protocol.hpp>
#include <asio/connect.hpp>

#include <cocaine/detail/service/node/messages.hpp>

#include "cocaine/framework/forwards.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

options_t::options_t(int argc, char** argv) {
    // TODO: Make help.
    boost::program_options::options_description options("Configuration");
    options.add_options()
        ("app",      boost::program_options::value<std::string>())
        ("uuid",     boost::program_options::value<std::string>())
        ("endpoint", boost::program_options::value<std::string>())
        ("locator",  boost::program_options::value<std::string>());


    boost::program_options::command_line_parser parser(argc, argv);
    parser.options(options);
    parser.allow_unregistered();

    boost::program_options::variables_map vm;
    boost::program_options::store(parser.run(), vm);
    boost::program_options::notify(vm);

    if (vm.count("app") == 0 || vm.count("uuid") == 0 || vm.count("endpoint") == 0 || vm.count("locator") == 0) {
        throw std::invalid_argument("invalid command line options");
    }

    name = vm["app"].as<std::string>();
    uuid = vm["uuid"].as<std::string>();
    endpoint = vm["endpoint"].as<std::string>();
    locator = vm["locator"].as<std::string>();
}

worker_t::worker_t(options_t options) :
    options(std::move(options)),
    heartbeat_timer(loop),
    disown_timer(loop)
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
    std::unique_ptr<socket_type> socket(new socket_type(loop));
    protocol_type::endpoint endpoint(options.endpoint);
    socket->connect(endpoint);

    channel.reset(new channel_type(std::move(socket)));

    channel->reader->read(message, std::bind(&worker_t::on_read, this, ph::_1));
    channel->writer->write(io::encoded<io::rpc::handshake>(1, options.uuid), std::bind(&worker_t::on_write, this, ph::_1));

    send_heartbeat(std::error_code());

    // The main thread is guaranteed to work only with cocaine socket and timers.
    loop.run();

    return 0;
}

void worker_t::stop() {
    loop.stop();
}

void worker_t::dispatch(const io::decoder_t::message_type& message) {
    // TODO: Make pretty.
    // visit(message);
    switch (message.type()) {
    case (io::event_traits<io::rpc::invoke>::id):
        // Find handler.
        // If not found - log and drop.
        // Create tx and rx.
        // Save shared state for rx. Pass this for tx.
        // Invoke handler.
        break;
    case (io::event_traits<io::rpc::chunk>::id):
    case (io::event_traits<io::rpc::error>::id):
    case (io::event_traits<io::rpc::choke>::id):
        break;
    default:
        COCAINE_ASSERT(false);
    }
}

void worker_t::on_read(const std::error_code& ec) {
    CF_DBG("read event: %s", CF_EC(ec));
    // TODO: Stop the worker on any network error.
    // TODO: If message.type() == 3, 4, 5, 6 => push. Otherwise unpack into the control message.
    switch (message.type()) {
    case (io::event_traits<io::rpc::handshake>::id):
        // TODO: Should be never sent.
        COCAINE_ASSERT(false);
        break;
    case (io::event_traits<io::rpc::heartbeat>::id):
        on_heartbeat();
        break;
    case (io::event_traits<io::rpc::terminate>::id):
        stop();
        break;
    case (io::event_traits<io::rpc::invoke>::id):
    case (io::event_traits<io::rpc::chunk>::id):
    case (io::event_traits<io::rpc::error>::id):
    case (io::event_traits<io::rpc::choke>::id):
        dispatch(message);
        break;
    default:
        break;
    }

    channel->reader->read(message, std::bind(&worker_t::on_read, this, ph::_1));
}

void worker_t::on_write(const std::error_code& ec) {
    // TODO: Stop the worker on any network error.
}

// TODO: Rename to: health_manager::reset
void worker_t::send_heartbeat(const std::error_code& ec) {
    if (ec) {
        // TODO: Handle the error properly.
        COCAINE_ASSERT(false);
        return;
    }

    CF_DBG("<- ♥");
    channel->writer->write(io::encoded<io::rpc::heartbeat>(1), std::bind(&worker_t::on_heartbeat_sent, this, ph::_1));
}

void worker_t::on_heartbeat_sent(const std::error_code& ec) {
    if (ec) {
        // TODO: Stop the worker.
        return;
    }

    heartbeat_timer.expires_from_now(boost::posix_time::seconds(10));
    heartbeat_timer.async_wait(std::bind(&worker_t::send_heartbeat, this, ph::_1));
}

void worker_t::on_disown(const std::error_code& ec) {
    if (ec) {
        if (ec == asio::error::operation_aborted) {
            // Okay. Do nothing.
            return;
        }

        // TODO: Handle other error types.
    }

    throw std::runtime_error("disowned");
}

void worker_t::on_heartbeat() {
    CF_DBG("-> ♥");

    disown_timer.expires_from_now(boost::posix_time::seconds(60));
    disown_timer.async_wait(std::bind(&worker_t::on_disown, this, ph::_1));
}
