#include "cocaine/framework/worker.hpp"

#include <csignal>

#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>

#include <asio/local/stream_protocol.hpp>
#include <asio/connect.hpp>

using namespace cocaine::framework;

namespace io = asio;

worker_t::worker_t(options_t options) {
    // TODO: Set default locator endpoint.
    // service_manager_t::endpoint_t locator_endpoint("127.0.0.1", 10053);

    // Block the deprecated signals.
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGPIPE);
    ::sigprocmask(SIG_BLOCK, &sigset, nullptr);
}

int worker_t::run() {
    io::io_service loop;
    io::local::stream_protocol::socket socket(loop);
    io::local::stream_protocol::endpoint endpoint("/");
    socket.connect(endpoint);

    return 0;
}

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
