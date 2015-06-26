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

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/worker/dispatch.hpp"
#include "cocaine/framework/worker/options.hpp"

namespace cocaine { namespace framework { namespace worker {

template<class Dispatch, class F>
struct transform_traits {
    typedef typename Dispatch::handler_type input_type;

    static
    typename Dispatch::handler_type
    apply(input_type handler) {
        return handler;
    }
};

}}} // namespace cocaine::framework::worker

namespace cocaine { namespace framework {

class worker_t {
public:
    typedef dispatch_t dispatch_type;
    typedef dispatch_type::handler_type handler_type;
    typedef dispatch_type::fallback_type fallback_type;

private:
    class impl;
    std::unique_ptr<impl> d;

public:
    explicit
    worker_t(options_t options);

    ~worker_t();

    template<class F>
    void
    on(std::string event, typename worker::transform_traits<dispatch_type, F>::input_type handler) {
        on(std::move(event), worker::transform_traits<dispatch_type, F>::apply(std::move(handler)));
    }

    service_manager_t&
    manager();

    void
    on(std::string event, handler_type handler);

    void
    fallback(fallback_type handler);

    auto
    options() const -> const options_t&;

    int
    run();
};

}} // namespace cocaine::framework
