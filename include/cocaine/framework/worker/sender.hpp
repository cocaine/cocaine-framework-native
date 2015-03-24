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
#include <string>

#include <cocaine/forwards.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

namespace worker {

class sender {
    std::shared_ptr<basic_sender_t<worker_session_t>> session;

public:
    /*!
     * \note this constructor is intentionally left implicit.
     */
    sender(std::shared_ptr<basic_sender_t<worker_session_t>> session);
    ~sender();

    sender(const sender& other) = delete;
    sender(sender&&) = default;

    sender& operator=(const sender& other) = delete;
    sender& operator=(sender&&) = default;

    auto write(std::string message) -> task<sender>::future_type;
    auto error(int id, std::string reason) -> task<void>::future_type;
    auto close() -> task<void>::future_type;
};

} // namespace worker

} // namespace framework

} // namespace cocaine
