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

#ifndef COCAINE_FRAMEWORK_SERVICES_LOGGING_HPP
#define COCAINE_FRAMEWORK_SERVICES_LOGGING_HPP

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/logging.hpp>

#include <cocaine/traits/enum.hpp>

namespace cocaine { namespace framework {

struct logging_service_t :
    public std::enable_shared_from_this<logging_service_t>,
    public service_t,
    public logger_t
{
    static const unsigned int version = cocaine::io::protocol<cocaine::io::logging_tag>::version::value;

    logging_service_t(std::shared_ptr<service_connection_t> connection,
                      const std::string& source) :
        service_t(connection),
        m_source(source),
        m_priority_updated(false)
    {
        try {
            m_priority = call<cocaine::io::logging::verbosity>().next();
            m_priority_updated = true;
        } catch (...) {
            m_priority = cocaine::logging::debug;
        }
    }

    void
    emit(cocaine::logging::priorities priority,
         const std::string& message)
    {
        if (status() != service_status::connected &&
            status() != service_status::connecting)
        {
            m_priority_updated = false;
        }

        if (!m_priority_updated) {
            call<cocaine::io::logging::verbosity>()
            .then(std::bind(&logging_service_t::set_verbosity,
                            shared_from_this(),
                            std::placeholders::_1));
        }

        call<cocaine::io::logging::emit>(priority, m_source, message);
    }

    cocaine::logging::priorities
    verbosity() const {
        return m_priority;
    }

private:
    void
    set_verbosity(generator<cocaine::logging::priorities>& g) {
        m_priority = g.next();
        m_priority_updated = true;
    }

private:
    std::string m_source;
    cocaine::logging::priorities m_priority;
    bool m_priority_updated;
};

}} // cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_LOGGING_HPP
