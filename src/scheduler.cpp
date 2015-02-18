#include "cocaine/framework/scheduler.hpp"

#include <blackhole/attribute/set.hpp>
#include <blackhole/scoped_attributes.hpp>

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/detail/loop.hpp"

using namespace cocaine::framework;

struct context_wrapper {
    scheduler_t::closure_type fn;
    blackhole::attribute::set_t set;

    context_wrapper(scheduler_t::closure_type fn) :
        fn(std::move(fn))
    {
        blackhole::scoped_attributes_t scoped(detail::logger(), blackhole::attribute::set_t());
        set = scoped.attributes();
    }

    void operator()() {
        blackhole::scoped_attributes_t scoped(detail::logger(), set);
        fn();
    }
};

void scheduler_t::operator()(closure_type fn) {
#ifdef CF_USE_INTERNAL_LOGGING
    ev.loop.post(context_wrapper(std::move(fn)));
#else
    ev.loop.post(std::move(fn));
#endif
}

namespace {

bool match_context(const blackhole::attribute::pair_t& pair) {
    return pair.first == "context";
}

}

namespace cocaine {

namespace framework {

boost::optional<std::string> current_context() {
    blackhole::scoped_attributes_t scoped(detail::logger(), blackhole::attribute::set_t());
    const auto& attributes = scoped.attributes();
    auto it = std::find_if(attributes.begin(), attributes.end(), &match_context);

    if (it == attributes.end()) {
        return boost::none;
    }

    return boost::get<std::string>(it->second.value);
}

struct context_holder::impl {
    blackhole::scoped_attributes_t* scoped;

    impl(std::string context) :
        scoped(context.empty() ? nullptr : new blackhole::scoped_attributes_t(detail::logger(), blackhole::attribute::set_t({{ "context", context }})))
    {}

    ~impl() {
        delete scoped;
    }
};

context_holder::context_holder(boost::optional<std::string> context) : d(new impl(context.get_value_or(""))) {}

context_holder::~context_holder() {}

}

}
