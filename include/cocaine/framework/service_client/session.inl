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

#ifndef COCAINE_FRAMEWORK_SERVICE_SESSION_INL
#define COCAINE_FRAMEWORK_SERVICE_SESSION_INL

template<class Event>
cocaine::framework::session<Event>::session(
    const std::shared_ptr<service_connection_t>& client,
    session_id_t id,
    future_type&& downstream
) :
    m_client(client),
    m_id(id),
    m_downstream(std::move(downstream))
{
    // pass
}

template<class Event>
cocaine::framework::session<Event>::session(cocaine::framework::session<Event>&& other) :
    m_client(std::move(other.m_client)),
    m_id(other.m_id),
    m_downstream(std::move(other.m_downstream))
{
    // pass
}

template<class Event>
cocaine::framework::session<Event>&
cocaine::framework::session<Event>::operator=(cocaine::framework::session<Event>&& other) {
    m_client = std::move(other.m_client);
    m_id = other.m_id;
    m_downstream = std::move(other.m_downstream);

    return *this;
}

template<class Event>
cocaine::framework::session_id_t
cocaine::framework::session<Event>::id() const {
    return m_id;
}

template<class Event>
void
cocaine::framework::session<Event>::set_timeout(float seconds) {
    m_client->set_timeout(m_id, seconds);
}

template<class Event>
typename cocaine::framework::session<Event>::future_type&
cocaine::framework::session<Event>::downstream() {
    return m_downstream;
}

#endif // COCAINE_FRAMEWORK_SERVICE_SESSION_INL
