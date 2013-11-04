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

#ifndef COCAINE_FRAMEWORK_SERVICE_SESSION_HPP
#define COCAINE_FRAMEWORK_SERVICE_SESSION_HPP

#include <cocaine/framework/service_client/handler.hpp>

#include <memory>

namespace cocaine { namespace framework {

typedef uint64_t
        session_id_t;

class service_connection_t;

template<class Event>
class session {
    COCAINE_DECLARE_NONCOPYABLE(session)

public:
    typedef typename detail::service::service_handler<Event>::future_type
            future_type;

public:
    session(const std::shared_ptr<service_connection_t>& client,
            session_id_t id,
            future_type&& downstream);

    session(session&& other);

    session&
    operator=(session&& other);

    session_id_t
    id() const;

    void
    set_timeout(float seconds);

    future_type&
    downstream();

private:
    std::shared_ptr<service_connection_t> m_client;
    session_id_t m_id;
    future_type m_downstream;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_SESSION_HPP
