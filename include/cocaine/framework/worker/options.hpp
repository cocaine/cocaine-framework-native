#pragma once

#include <string>

namespace cocaine {

namespace framework {

struct options_t {
    std::string name;
    std::string uuid;
    std::string endpoint;
    std::string locator;

    /// Parses command-line arguments to extract all required settings to be able to start the
    /// worker.
    ///
    /// Can internally exit on invalid command-line arguments, providing an help message and
    /// returning proper exit code.
    options_t(int argc, char** argv);
};

}

}
