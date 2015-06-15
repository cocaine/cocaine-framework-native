/*
    Copyright (c) 2015 Evgeny Safronov <division494@gmail.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.
    This file is part of Cocaine.
    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/framework/config.hpp"

#ifdef COCAINE_FRAMEWORK_HAS_INTERNAL_TRACING
#include "cocaine/framework/trace/enabled.hpp"

#include "cocaine/framework/detail/log.hpp"

#include <blackhole/attribute/set.hpp>
#include <blackhole/scoped_attributes.hpp>

using namespace cocaine::framework::trace;

namespace {

bool
match_context(const blackhole::attribute::pair_t& pair) {
    return pair.first == "context";
}

} // namespace

namespace cocaine { namespace framework { namespace trace {

std::string
current_context() {
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
        if (scoped) {
            delete scoped;
        }
    }
};

context_holder::context_holder(const std::string& context) :
    d(new impl(context))
{}

context_holder::~context_holder()
{}

}}} // namespace cocaine::framework::trace
#endif
