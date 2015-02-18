#pragma once

#include <functional>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

struct context_holder {
    struct impl;
    std::unique_ptr<impl> d;

    context_holder(boost::optional<std::string>);
    ~context_holder();
};

boost::optional<std::string> current_context();

template<typename F>
class callable {
public:
    typedef F function_type;

private:
    boost::optional<std::string> context;
    function_type fn;

public:
    callable(function_type fn) :
        context(current_context()),
        fn(std::move(fn))
    {}

    template<typename... Args>
    void operator()(Args&&... args) {
        context_holder holder(context);
        fn(std::forward<Args>(args)...);
    }
};

template<typename F>
callable<F> wrap(F&& f) {
    return callable<F>(std::forward<F>(f));
}

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
