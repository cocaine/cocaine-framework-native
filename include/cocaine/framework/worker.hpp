#pragma once

namespace cocaine {

namespace framework {

struct options_t {};

class worker_t {
public:
    worker_t(int argc, char** argv, options_t options = options_t());

    int run();
};

} // namespace framework

} // namespace cocaine
