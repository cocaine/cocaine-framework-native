#include "cocaine/framework/worker/options.hpp"

#include <boost/program_options.hpp>

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

    if (vm.count("app") == 0 ||
        vm.count("uuid") == 0 ||
        vm.count("endpoint") == 0 ||
        vm.count("locator") == 0)
    {
        throw std::invalid_argument("invalid command line options");
    }

    name     = vm["app"].as<std::string>();
    uuid     = vm["uuid"].as<std::string>();
    endpoint = vm["endpoint"].as<std::string>();
    locator  = vm["locator"].as<std::string>();
}
