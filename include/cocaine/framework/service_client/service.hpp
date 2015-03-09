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

#ifndef COCAINE_FRAMEWORK_SERVICE_BASE_HPP
#define COCAINE_FRAMEWORK_SERVICE_BASE_HPP

#include <cocaine/framework/service_client/connection.hpp>

#include <boost/optional.hpp>

#include <memory>
#include <string>

namespace cocaine { namespace framework {

template<class Event>
struct service_traits {
    typedef typename detail::service::service_handler<Event>::future_type
            future_type;

    typedef typename detail::service::service_handler<Event>::promise_type
            promise_type;
};

class service_t {
    COCAINE_DECLARE_NONCOPYABLE(service_t)

public:
    service_t(std::shared_ptr<service_connection_t> connection);

    virtual
    ~service_t();

    std::string
    name();

    service_status
    status() const;

    void
    set_timeout(float timeout);

    template<class Event, typename... Args>
    typename session<Event>::future_type
    call(Args&&... args);

    void
    soft_destroy();

private:
    std::shared_ptr<service_connection_t> m_connection;
    boost::optional<float> m_timeout;
};

template<class Tag>
class service :
    public service_t
{
public:
    static const unsigned int version = cocaine::io::protocol<Tag>::version::value;

    service(std::shared_ptr<service_connection_t> connection) :
        service_t(connection)
    {
        // pass
    }
};

}} // namespace cocaine::framework

template<class Event, typename... Args>
typename cocaine::framework::session<Event>::future_type
cocaine::framework::service_t::call(Args&&... args) {
    auto session = m_connection->call<Event, Args...>(std::forward<Args>(args)...);

    if (m_timeout) {
        session.set_timeout(*m_timeout);
    }

    return std::move(session.downstream());
}

#endif // COCAINE_FRAMEWORK_SERVICE_BASE_HPP
