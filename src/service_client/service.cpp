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

#include <cocaine/framework/service.hpp>

/*
 * error_code
 */

namespace {

    struct service_client_category_impl :
        public std::error_category
    {
        const char*
        name() const throw() {
            return "service client";
        }

        std::string
        message(int error_value) const throw() {
            switch (error_value) {
                case static_cast<int>(cocaine::framework::service_errc::bad_version):
                    return "Bad version of service";

                case static_cast<int>(cocaine::framework::service_errc::not_connected):
                    return "There is no connection to service";

                case static_cast<int>(cocaine::framework::service_errc::not_found):
                    return "Service not found in locator";

                case static_cast<int>(cocaine::framework::service_errc::wait_for_connection):
                    return "Client is reconnecting to service at now";

                case static_cast<int>(cocaine::framework::service_errc::broken_manager):
                    return "Service manager no longer exists";

                case static_cast<int>(cocaine::framework::service_errc::timeout):
                    return "Session timed out";

                default:
                    return "Unknown service error o_O";
            }
        }
    };

    struct service_response_category_impl :
        public std::error_category
    {
        const char*
        name() const throw() {
            return "service response";
        }

        std::string
        message(int /* error_value */) const throw() {
            return "Error from service";
        }
    };

    service_client_category_impl client_category;
    service_response_category_impl response_category;

} // namespace

const std::error_category&
cocaine::framework::service_client_category() {
    return client_category;
}

const std::error_category&
cocaine::framework::service_response_category() {
    return response_category;
}

std::error_code
cocaine::framework::make_error_code(cocaine::framework::service_errc e) {
    return std::error_code(static_cast<int>(e), cocaine::framework::service_client_category());
}

std::error_condition
cocaine::framework::make_error_condition(cocaine::framework::service_errc e) {
    return std::error_condition(static_cast<int>(e), cocaine::framework::service_client_category());
}

/*
 * service_t
 */

using namespace cocaine::framework;

service_t::service_t(std::shared_ptr<service_connection_t> connection) :
    m_connection(connection)
{
    // pass
}

service_t::~service_t() {
    if (m_connection) {
        m_connection->auto_reconnect(false);
        m_connection->reactor().post(std::bind(&service_connection_t::disconnect,
                                               std::move(m_connection),
                                               service_status::disconnected));
    }
}

void
service_t::set_timeout(float timeout) {
    m_timeout = timeout;
}

std::string
service_t::name() {
    return m_connection->name();
}

service_status
service_t::status() const {
    return m_connection->status();
}

namespace {
    template<class... Args>
    struct emptyf {
        static
        void
        call(Args...){
            // pass
        }
    };
}

void
service_t::soft_destroy() {
    m_connection->auto_reconnect(false);
    m_connection->reactor().post(std::bind(&emptyf<std::shared_ptr<service_connection_t>>::call,
                                           std::move(m_connection)));
}
