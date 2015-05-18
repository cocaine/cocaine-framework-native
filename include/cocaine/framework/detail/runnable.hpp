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

#if defined(__linux__)
#include <sys/prctl.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

#include <string>

#include <cocaine/framework/detail/log.hpp>

namespace cocaine {

namespace framework {

namespace detail {

template<class Loop>
class named_runnable {
public:
    typedef Loop loop_type;

private:
    const char* name;
    loop_type& loop;

public:
    template<size_t N>
    named_runnable(const char(&name)[N], loop_type& loop):
        name(name),
        loop(loop)
    {
        static_assert(N <= 16, "a thread name must fit in 16 bytes including the terminate null byte");
    }

    void operator()() {
#if defined(__linux__)
        ::prctl(PR_SET_NAME, name);
#elif defined(__APPLE__)
        ::pthread_setname_np(name);
#endif

        loop.run();
    }
};

} // namespace detail

} // namespace framework

} // namespace cocaine
