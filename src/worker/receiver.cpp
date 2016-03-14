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

auto on_recv(const decoded_message& message) -> boost::optional<std::string> {
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

auto on_recv_data(task<decoded_message>::future_move_type future) -> boost::optional<std::string> {
    return on_recv(future.get());
}

auto on_recv_with_meta(future<decoded_message>& future) -> boost::optional<frame_t> {
    const auto message = future.get();
    if (auto chunk = on_recv(message)) {
        // TODO(@antmat): WTF?
        hpack::header_storage_t headers;

        for (const auto& header : message.meta()) {
            headers.push_back(header);
        }

        return boost::optional<frame_t>({*chunk, std::move(headers)});
    }

    return boost::none;
}

} // namespace

namespace cocaine {
namespace framework {
namespace worker {

receiver::receiver(const std::vector<hpack::header_t>& headers,
                   std::shared_ptr<basic_receiver_t<worker_session_t>> session) :
    headers(headers),
    session(std::move(session))
{}

auto receiver::invocation_headers() const noexcept -> const hpack::header_storage_t& {
    return headers;
}

template<>
auto receiver::recv<std::string>() -> future<boost::optional<std::string>> {
    return session->recv()
        .then(std::bind(&on_recv_data, ph::_1));
}

template<>
auto receiver::recv<frame_t>() -> future<boost::optional<frame_t>> {
    return session->recv()
        .then(std::bind(&on_recv_with_meta, ph::_1));
}

}  // namespace worker
}  // namespace framework
}  // namespace cocaine
