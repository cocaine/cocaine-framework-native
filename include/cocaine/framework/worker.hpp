#pragma once

#include <functional>
#include <memory>
#include <string>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/worker/dispatch.hpp"
#include "cocaine/framework/worker/options.hpp"

namespace cocaine {

namespace framework {

template<class Dispatch, class F>
struct transform_traits {
    static
    typename Dispatch::handler_type
    apply(F handler) {
        return handler;
    }
};

class worker_t {
public:
    typedef dispatch_t dispatch_type;

    typedef dispatch_type::sender_type   sender_type;
    typedef dispatch_type::receiver_type receiver_type;
    typedef dispatch_type::handler_type  handler_type;

private:
    class impl;
    std::unique_ptr<impl> d;

public:
    explicit worker_t(options_t options);

    ~worker_t();

    template<class F>
    void on(std::string event, F handler) {
        handler_type mapped = transform_traits<dispatch_type, F>::apply(std::move(handler));
        on(event, std::move(mapped));
    }

    void on(std::string event, handler_type handler);

    auto options() const -> const options_t&;

    int run();
};

} // namespace framework

} // namespace cocaine
