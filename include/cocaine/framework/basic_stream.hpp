/*
Copyright (c) 2013 Andrey Goryachev <andrey.goryachev@gmail.com>
Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_FRAMEWORK_BASIC_STREAM_HPP
#define COCAINE_FRAMEWORK_BASIC_STREAM_HPP

#include <cocaine/framework/common.hpp>

#include <string>
#include <exception>
#include <system_error>

namespace cocaine { namespace framework {

template<class T>
class basic_stream {
public:
    virtual
    void
    write(T&&) = 0;

    virtual
    void
    error(const std::exception_ptr& e) = 0;

    virtual
    void
    error(const std::system_error& err) {
        error(cocaine::framework::make_exception_ptr(err));
    }

    virtual
    void
    close() = 0;

    virtual
    bool
    closed() const = 0;

    virtual
    ~basic_stream() {
        // pass
    }
};

template<>
class basic_stream<void> {
public:
    virtual
    void
    error(const std::exception_ptr& e) = 0;

    virtual
    void
    error(const std::system_error& err) {
        error(cocaine::framework::make_exception_ptr(err));
    }

    virtual
    void
    close() = 0;

    virtual
    bool
    closed() const = 0;

    virtual
    ~basic_stream() {
        // pass
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_BASIC_STREAM_HPP
