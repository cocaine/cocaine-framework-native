#pragma once

#include <functional>
#include <memory>
#include <string>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/worker/dispatch.hpp"
#include "cocaine/framework/worker/options.hpp"

namespace cocaine {

namespace framework {

namespace worker {

template<class Dispatch, class F>
struct transform_traits {
    typedef typename Dispatch::handler_type input_type;

    static
    typename Dispatch::handler_type
    apply(input_type handler) {
        return handler;
    }
};

} // namespace worker

class worker_t {
public:
    typedef dispatch_t dispatch_type;
    typedef dispatch_type::handler_type handler_type;

private:
    class impl;
    std::unique_ptr<impl> d;

public:
    explicit worker_t(options_t options);

    ~worker_t();

    template<class F>
    void
    on(std::string event, typename worker::transform_traits<dispatch_type, F>::input_type handler) {
        on(std::move(event), worker::transform_traits<dispatch_type, F>::apply(std::move(handler)));
    }

    service_manager_t& manager();

    void on(std::string event, handler_type handler);

    auto options() const -> const options_t&;

    int run();
};

} // namespace framework

} // namespace cocaine
