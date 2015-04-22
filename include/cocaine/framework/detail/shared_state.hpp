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

#include <queue>

#include "cocaine/framework/trace.hpp"

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/message.hpp"

#include "cocaine/framework/detail/decoder.hpp"

namespace cocaine {

namespace framework {

/// \internal
class shared_state_t {
public:
    typedef decoded_message value_type;

private:
    std::queue<value_type> queue;
    std::queue<task<value_type>::promise_type> await;

    boost::optional<std::error_code> broken;

    std::mutex mutex;

public:
    shared_state_t() :
        trace(trace_t::current())
    {}
    void put(value_type&& message);
    void put(const std::error_code& ec);
    auto get() -> task<value_type>::future_type;

    trace_t trace;
};

} // namespace framework

} // namespace cocaine
