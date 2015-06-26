#pragma once

#include <string>
#include <type_traits>

namespace cocaine { namespace framework { namespace trace {

struct context_holder {
    explicit
    context_holder(const std::string&) {}
};

template<typename F>
F wrap(F&& f) {
    return std::forward<F>(f);
}

}}} // namespace cocaine::framework::trace
