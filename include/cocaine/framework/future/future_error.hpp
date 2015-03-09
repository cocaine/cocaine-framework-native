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

#ifndef COCAINE_FRAMEWORK_FUTURE_ERROR_HPP
#define COCAINE_FRAMEWORK_FUTURE_ERROR_HPP

#include <system_error>
#include <stdexcept>
#include <exception>
#include <type_traits>
#include <string>

namespace cocaine { namespace framework {

enum class future_errc {
    broken_promise,
    future_already_retrieved,
    promise_already_satisfied,
    no_state,
    stream_closed
};

const std::error_category&
future_category();

std::error_code
make_error_code(future_errc e);

std::error_condition
make_error_condition(future_errc e);

struct future_error :
    public std::logic_error
{
    future_error (std::error_code ec) :
        std::logic_error(ec.message()),
        m_ec(ec)
    {
        // pass
    }

    const std::error_code&
    code() const {
        return m_ec;
    }

private:
    std::error_code m_ec;
};

}} // namespace cocaine::framework

namespace std {

template<>
struct is_error_code_enum<cocaine::framework::future_errc> :
    public true_type
{
    // pass
};

} // namespace std

#endif // COCAINE_FRAMEWORK_FUTURE_ERROR_HPP
