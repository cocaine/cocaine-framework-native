#pragma once

#include <functional>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

class scheduler_t {
public:
    typedef std::function<void()> closure_type;

private:
    event_loop_t& ev;

public:
    /*!
     * \note must be created inside service manager or worker.
     */
    explicit scheduler_t(event_loop_t& loop) :
        ev(loop)
    {}

    void operator()(closure_type fn);

    event_loop_t& loop() {
        return ev;
    }
};

}

}
