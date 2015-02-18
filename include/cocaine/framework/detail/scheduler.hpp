#pragma once

#include <functional>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

class scheduler_t {
public:
    typedef std::function<void()> closure_type;

private:
    loop_t& loop_;

public:
    explicit scheduler_t(loop_t& loop) :
        loop_(loop)
    {}

    void operator()(closure_type fn) {
        loop_.post(fn);
    }

    loop_t& loop() {
        return loop_;
    }
};

}

}
