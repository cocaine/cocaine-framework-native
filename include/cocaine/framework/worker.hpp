#pragma once

#include <string>

namespace cocaine {

namespace framework {

struct options_t {
    std::string name;
    std::string uuid;
    std::string endpoint;
    std::string locator;

    options_t(int argc, char** argv);
};

class worker_t {
public:
    worker_t(options_t options);

    int run();
};

} // namespace framework

} // namespace cocaine
