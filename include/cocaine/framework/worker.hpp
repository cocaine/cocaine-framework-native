#pragma once

#include <functional>
#include <memory>
#include <string>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/worker/dispatch.hpp"
#include "cocaine/framework/worker/options.hpp"

namespace cocaine {

namespace framework {

class worker_t {
public:
    typedef dispatch_t dispatch_type;

    typedef dispatch_type::sender_type   sender_type;
    typedef dispatch_type::receiver_type receiver_type;
    typedef dispatch_type::handler_type  handler_type;

private:
    class impl;
    std::unique_ptr<impl> d;

    dispatch_type dispatch;

public:
    explicit worker_t(options_t options);

    ~worker_t();

    template<class F>
    void on(std::string event, F handler) {
        dispatch.on(event, std::move(handler));
    }

    auto options() const -> const options_t&;

    int run();
};

} // namespace framework

} // namespace cocaine
