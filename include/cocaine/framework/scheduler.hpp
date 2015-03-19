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

#include "cocaine/framework/config.hpp"
#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

struct context_holder {
#ifdef CF_USE_INTERNAL_LOGGING
    class impl;
    std::unique_ptr<impl> d;
#endif

    explicit context_holder(const std::string&);
    ~context_holder();
};

#ifdef CF_USE_INTERNAL_LOGGING

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

#else

template<typename F>
F wrap(F&& f) {
    return std::forward<F>(f);
}

#endif

class scheduler_t {
public:
    typedef std::function<void()> closure_type;

private:
    event_loop_t& ev;

public:
    /*!
     * \note must be created inside a service manager or a worker.
     */
    explicit scheduler_t(event_loop_t& loop) :
        ev(loop)
    {}

    void operator()(closure_type fn);

    event_loop_t& loop() {
        return ev;
    }
};

}

}
