#include "cocaine/framework/worker/options.hpp"

#include <array>
#include <iostream>

#include <boost/program_options.hpp>

using namespace cocaine::framework;

namespace {

void help(const char* program, const boost::program_options::options_description& description) {
    std::cerr << "Usage: " << program
              << " --app APP --uuid UUID --endpoint ENDPOINT --locator LOCATOR"
              << std::endl
              << std::endl;
    std::cerr << description << std::endl;
}

} // namespace

options_t::options_t(int argc, char** argv) {
    boost::program_options::options_description options("Configuration");
    options.add_options()
        ("app",      boost::program_options::value<std::string>(), "application name")
        ("uuid",     boost::program_options::value<std::string>(), "worker uuid")
        ("endpoint", boost::program_options::value<std::string>(), "cocaine-runtime endpoint")
        ("locator",  boost::program_options::value<std::string>(), "locator endpoint");

    boost::program_options::options_description general("General options");
    general.add(options);
    general.add_options()
        ("help,h",     "display this help and exit")
        ("version,v",  "output the Framework version information and exit");

    boost::program_options::command_line_parser parser(argc, argv);
    parser.options(general);

    boost::program_options::variables_map vm;
    boost::program_options::store(parser.run(), vm);
    boost::program_options::notify(vm);

    if (vm.count("help")) {
        help(argv[0], general);
        std::exit(0);
    }

    if (vm.count("version")) {
        std::cerr << "0.12.0" << std::endl;
        std::exit(0);
    }

    const std::array<const char*, 4> required = {{ "app", "uuid", "endpoint", "locator" }};
    for (uint id = 0; id < required.size(); ++id) {
        auto option = required[id];

        if (vm.count(option) == 0) {
            std::cerr << "ERROR: the required '" << option << "' option is not specified"
                      << std::endl << std::endl;
            help(argv[0], general);
            std::exit(id);
        }
    }

    name     = vm["app"].as<std::string>();
    uuid     = vm["uuid"].as<std::string>();
    endpoint = vm["endpoint"].as<std::string>();
    locator  = vm["locator"].as<std::string>();
}
