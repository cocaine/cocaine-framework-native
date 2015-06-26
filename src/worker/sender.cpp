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

#include "cocaine/framework/worker/sender.hpp"

#include <cocaine/idl/rpc.hpp>
#include <cocaine/traits/error_code.hpp>

#include "cocaine/framework/sender.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

namespace {

worker::sender
on_write(task<void>::future_move_type future, std::shared_ptr<basic_sender_t<worker_session_t>> session) {
    future.get();
    return worker::sender(session);
}

void
on_error(task<void>::future_move_type future) {
    future.get();
}

void
on_close(task<void>::future_move_type future) {
    future.get();
}

} // namespace

worker::sender::sender(std::shared_ptr<basic_sender_t<worker_session_t>> session) :
    session(std::move(session))
{}

worker::sender::~sender() {
    // Close this stream if it wasn't explicitly closed.
    if (session) {
        close();
    }
}

auto worker::sender::write(std::string message) -> task<worker::sender>::future_type {
    auto session = std::move(this->session);

    return session->send<io::rpc::chunk>(std::move(message))
        .then(std::bind(&on_write, ph::_1, session));
}

auto worker::sender::error(int id, std::string reason) -> task<void>::future_type {
    auto session = std::move(this->session);

    return session->send<io::rpc::error>(std::error_code(id, std::system_category()), std::move(reason))
        .then(std::bind(&on_error, ph::_1));
}

auto worker::sender::close() -> task<void>::future_type {
    auto session = std::move(this->session);

    return session->send<io::rpc::choke>()
        .then(std::bind(&on_close, ph::_1));
}
