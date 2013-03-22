/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include <memory>

#include <cocaine/essentials/services/logging.hpp>

#include <cocaine/framework/logger.hpp>

using namespace cocaine::framework;

remote_t::remote_t(const std::string& name,
                   const Json::Value& args,
                   cocaine::io::service_t& service)
{
    auto socket_ = std::make_shared<cocaine::io::socket<cocaine::io::tcp>>(
        cocaine::io::tcp::endpoint("127.0.0.1", 12501)
    );

    m_encoder.attach(
        std::make_shared<
            cocaine::io::writable_stream<cocaine::io::socket<cocaine::io::tcp>>
        >(service, socket_)
    );
}

void
remote_t::emit(cocaine::logging::priorities priority,
               const std::string& source,
               const std::string& message)
{
    m_encoder.write<cocaine::io::logging::emit>(
        0ul,
        static_cast<int>(priority),
        source,
        message
    );
}
