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

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <cocaine/forwards.hpp>
#include <cocaine/rpc/protocol.hpp>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/worker/sender.hpp"
#include "cocaine/framework/worker/receiver.hpp"

namespace cocaine { namespace framework {

class dispatch_t {
public:
    typedef std::function<void(worker::sender, worker::receiver)> handler_type;
    typedef std::function<void(const std::string&, worker::sender, worker::receiver)> fallback_type;

private:
    std::unordered_map<std::string, handler_type> handlers;

    struct {
        fallback_type fallback;
    } data;

public:
    dispatch_t();

    boost::optional<handler_type>
    get(const std::string& event) const;

    void
    on(std::string event, handler_type handler);

    fallback_type
    fallback() const;

    void
    fallback(fallback_type handler);
};

}} // namespace cocaine::framework
