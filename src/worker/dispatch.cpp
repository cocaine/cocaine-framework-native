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

#include "cocaine/framework/worker/dispatch.hpp"

#include "cocaine/service/node/error.hpp"

using namespace cocaine::framework;

namespace {

void
default_fallback(const std::string& event, worker::sender tx, worker::receiver) {
    const auto reason = "event '" + event + "' not found";

    tx.error(make_error_code(cocaine::service::node::event_not_found), reason);
}

} // namespace

dispatch_t::dispatch_t() {
    data.fallback = &default_fallback;
}

boost::optional<dispatch_t::handler_type>
dispatch_t::get(const std::string& event) const {
    auto it = handlers.find(event);
    if (it != handlers.end()) {
        return it->second;
    }

    return boost::none;
}

void
dispatch_t::on(std::string event, dispatch_t::handler_type handler) {
    handlers[event] = std::move(handler);
}

dispatch_t::fallback_type
dispatch_t::fallback() const {
    return data.fallback;
}

void
dispatch_t::fallback(fallback_type handler) {
    data.fallback = std::move(handler);
}
