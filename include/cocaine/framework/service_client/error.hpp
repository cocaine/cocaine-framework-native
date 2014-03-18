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

#ifndef COCAINE_FRAMEWORK_SERVICE_ERROR_HPP
#define COCAINE_FRAMEWORK_SERVICE_ERROR_HPP

#include <system_error>
#include <stdexcept>
#include <string>
#include <exception>
#include <type_traits>

namespace cocaine { namespace framework {

enum class service_errc {
    bad_version,
    not_connected,
    not_found,
    wait_for_connection,
    broken_manager,
    timeout
};

const std::error_category&
service_client_category();

const std::error_category&
service_response_category();

std::error_code
make_error_code(service_errc e);

std::error_condition
make_error_condition(service_errc e);

struct service_error_t :
    public std::runtime_error
{
    service_error_t (std::error_code ec) :
        std::runtime_error(ec.message()),
        m_ec(ec)
    {
        // pass
    }
    service_error_t (std::error_code ec,
                     const std::string& message) :
        std::runtime_error(message),
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
struct is_error_code_enum<cocaine::framework::service_errc> :
    public true_type
{
    // pass
};

} // namespace std

#endif // COCAINE_FRAMEWORK_SERVICE_ERROR_HPP
