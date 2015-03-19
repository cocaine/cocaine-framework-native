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

#include <memory>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/session.hpp"

namespace cocaine {

namespace framework {

class execution_unit_t;

class service_manager_t {
    size_t current;
    std::vector<session_t::endpoint_type> locations;
    std::vector<std::unique_ptr<execution_unit_t>> units;

public:
    service_manager_t();
    explicit service_manager_t(unsigned int threads);

    ~service_manager_t();

    std::vector<session_t::endpoint_type> endpoints() const;
    void endpoints(std::vector<session_t::endpoint_type> endpoints);

    template<class T>
    service<T>
    create(std::string name) {
        return service<T>(std::move(name), endpoints(), next());
    }

private:
    void start(unsigned int threads);

    scheduler_t& next();
};

} // namespace framework

} // namespace cocaine
