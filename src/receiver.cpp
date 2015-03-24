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

#include "cocaine/framework/receiver.hpp"

#include "cocaine/framework/session.hpp"

#include "cocaine/framework/detail/basic_session.hpp"
#include "cocaine/framework/detail/shared_state.hpp"
#include "cocaine/framework/detail/log.hpp"

using namespace cocaine::framework;

template<class Session>
basic_receiver_t<Session>::basic_receiver_t(std::uint64_t id, std::shared_ptr<Session> session, std::shared_ptr<shared_state_t> state) :
    id(id),
    session(std::move(session)),
    state(std::move(state))
{}

template<class Session>
basic_receiver_t<Session>::~basic_receiver_t() {
    CF_DBG("revoking ...");
    session->revoke(id);
}

template<class Session>
task<decoded_message>::future_type
basic_receiver_t<Session>::recv() {
    return state->get();
}
