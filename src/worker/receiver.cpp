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

#include "cocaine/framework/worker/receiver.hpp"

#include <cocaine/idl/rpc.hpp>
#include <cocaine/traits/error_code.hpp>
#include <cocaine/traits/tuple.hpp>

#include "cocaine/framework/message.hpp"
#include "cocaine/framework/receiver.hpp"
#include "cocaine/framework/worker/error.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;
using namespace cocaine::framework::worker;

typedef io::protocol<io::worker::rpc::invoke::upstream_type>::scope protocol;

namespace {

boost::optional<std::string>
on_recv(task<decoded_message>::future_move_type future) {
    const auto message = future.get();
    const auto id = message.type();
    switch (id) {
    case io::event_traits<protocol::chunk>::id: {
        std::string chunk;
        io::type_traits<
            io::event_traits<protocol::chunk>::argument_type
        >::unpack(message.args(), chunk);
        return chunk;
    }
    case io::event_traits<protocol::error>::id: {
        std::error_code ec;
        std::string reason;
        io::type_traits<
            io::event_traits<protocol::error>::argument_type
        >::unpack(message.args(), ec, reason);
        throw request_error(std::move(ec), std::move(reason));
    }
    case io::event_traits<protocol::choke>::id:
        return boost::none;
    default:
        throw invalid_protocol_type(id);
    }

    return boost::none;
}

} // namespace

worker::receiver::receiver(std::shared_ptr<basic_receiver_t<worker_session_t>> session) :
    session(std::move(session))
{}

auto worker::receiver::recv() -> task<boost::optional<std::string>>::future_type {
    return session->recv()
        .then(std::bind(&on_recv, ph::_1));
}
