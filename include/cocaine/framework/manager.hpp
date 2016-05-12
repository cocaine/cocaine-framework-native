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

#include <cstdint>
#include <memory>
#include <string>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/session.hpp"

namespace cocaine { namespace io {
    struct log_tag;
}} // namespace cocaine::io

namespace cocaine { namespace framework {

class service_manager_data;
class service_manager_t {
public:
    typedef session_t::endpoint_type endpoint_type;

private:
    std::unique_ptr<service_manager_data> d;

public:
    /// Constructs the service manager using default settings.
    service_manager_t();

    /// Constructs the service manager using the given number of worker threads.
    explicit
    service_manager_t(unsigned int threads);

    /// Constructs a service manager using the given entry points and number of worker threads.
    service_manager_t(std::vector<endpoint_type> entries, unsigned int threads);

    /// Constructs a new service manager after performing a blocking DNS resolving of a given
    /// locator endpoints.
    ///
    /// \param entries locator endpoints as a list of FQDN:port pairs.
    /// \param threads number of worker threads.
    service_manager_t(std::vector<std::tuple<std::string, std::uint16_t>> entries, unsigned int threads);

    ~service_manager_t();

    std::vector<endpoint_type>
    endpoints() const;

    template<class T>
    service<T>
    create(std::string name) {
        return service<T>(logger(), std::move(name), endpoints(), next());
    }

    /// Returns a shared pointer to the associated logger service.
    std::shared_ptr<service<io::log_tag>>
    logger() const;

private:
    void
    start(unsigned int threads);

    scheduler_t&
    next();
};

}} // namespace cocaine::framework
