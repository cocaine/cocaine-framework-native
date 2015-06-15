#pragma once

#include <memory>
#include <string>

#include "cocaine/framework/config.hpp"

namespace cocaine { namespace framework { namespace trace {

struct context_holder {
    class impl;
    std::unique_ptr<impl> d;

    explicit
    context_holder(const std::string& name);

    ~context_holder();
};

std::string current_context();

template<typename F>
class callable {
public:
    typedef F function_type;

private:
    std::string context;
    function_type fn;

public:
    callable(function_type fn) :
        context(current_context()),
        fn(std::move(fn))
    {}

    template<typename... Args>
    auto operator()(Args&&... args) -> decltype(fn(std::forward<Args>(args)...)) {
        context_holder holder(context);
        return fn(std::forward<Args>(args)...);
    }
};

template<typename F>
callable<F> wrap(F&& f) {
    return callable<F>(std::forward<F>(f));
}

}}} // namespace cocaine::framework::trace
