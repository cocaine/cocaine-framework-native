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

#include <cocaine/common.hpp>

#include "cocaine/framework/sender.hpp"
#include "cocaine/framework/session.hpp"

#include "cocaine/framework/detail/basic_session.hpp"

using namespace cocaine::framework;

template<class Session>
basic_sender_t<Session>::basic_sender_t(std::uint64_t id, std::shared_ptr<session_type> session) :
    id(id),
    session(std::move(session))
{}

template<class Session>
task<void>::future_type
basic_sender_t<Session>::send(const std::function<io::encoder_t::message_type(io::encoder_t&)>& message_getter) {
    return session->push(message_getter);
}
