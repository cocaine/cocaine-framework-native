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
    asio::io_service loop;
    std::unique_ptr<socket_type> socket(new socket_type(loop));
    protocol_type::endpoint endpoint(options.endpoint);
    socket->connect(endpoint);

    channel.reset(new channel_type(std::move(socket)));

    channel->reader->read(message, std::bind(&worker_t::on_read, this, ph::_1));
    channel->writer->write(io::encoded<io::rpc::handshake>(1, options.uuid), std::bind(&worker_t::on_write, this, ph::_1));

    loop.run();

    return 0;
}

void worker_t::on_read(const std::error_code& ec) {
    CF_DBG("read event: %s", CF_EC(ec));
    // TODO: Stop the worker with error on any network error.
    // TODO: If message.type() == 4, 5, 6 => push. Otherwise unpack into the control message.
}

void worker_t::on_write(const std::error_code& ec) {

}
