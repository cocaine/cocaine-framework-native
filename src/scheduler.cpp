#include "cocaine/framework/scheduler.hpp"

#ifdef CF_USE_INTERNAL_LOGGING
#include <blackhole/attribute/set.hpp>
#include <blackhole/scoped_attributes.hpp>
#endif

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/detail/loop.hpp"

using namespace cocaine::framework;

void scheduler_t::operator()(closure_type fn) {
    ev.loop.post(std::move(fn));
}

#ifdef CF_USE_INTERNAL_LOGGING

namespace {

bool match_context(const blackhole::attribute::pair_t& pair) {
    return pair.first == "context";
}

} // namespace

#endif

namespace cocaine {

namespace framework {

#ifdef CF_USE_INTERNAL_LOGGING

std::string current_context() {
    blackhole::scoped_attributes_t scoped(detail::logger(), blackhole::attribute::set_t());
    const auto& attributes = scoped.attributes();
    auto it = std::find_if(attributes.begin(), attributes.end(), &match_context);

    if (it == attributes.end()) {
        return "";
    }

    return boost::get<std::string>(it->second.value);
}

class context_holder::impl {
public:
    blackhole::scoped_attributes_t* scoped;

    impl(std::string context) :
        scoped(context.empty() ? nullptr : new blackhole::scoped_attributes_t(detail::logger(), blackhole::attribute::set_t({{ "context", context }})))
    {}

    ~impl() {
        delete scoped;
    }
};

#endif

#ifdef CF_USE_INTERNAL_LOGGING
context_holder::context_holder(const std::string& context) : d(new impl(context)) {}
#else
context_holder::context_holder(const std::string&) {}
#endif

context_holder::~context_holder() {}

}

}
