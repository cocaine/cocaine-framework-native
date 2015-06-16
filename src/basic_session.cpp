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

#include "cocaine/framework/detail/basic_session.hpp"

#include <memory>

#include <asio/connect.hpp>

#include "cocaine/framework/sender.hpp"
#include "cocaine/framework/scheduler.hpp"
#include "cocaine/framework/trace.hpp"

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/detail/loop.hpp"
#include "cocaine/framework/detail/net.hpp"
#include "cocaine/framework/detail/shared_state.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;
using namespace cocaine::framework::detail;

/// \note single shot.
class basic_session_t::push_t :
    public std::enable_shared_from_this<push_t>
{
    const io::encoder_t::message_type message;

    // Keeps the session alive until all the operations are complete.
    const std::shared_ptr<basic_session_t> session;

    promise<void> pr;

public:
    push_t(io::encoder_t::message_type&& message,
           std::shared_ptr<basic_session_t> session,
           promise<void>&& pr) :
        message(std::move(message)),
        session(std::move(session)),
        pr(std::move(pr))
    {}

    void
    operator()(std::shared_ptr<channel_type> transport) {
        CF_DBG("writing %lu bytes ...", message.size());

        transport->writer->write(
            message,
            trace::wrap(std::bind(&push_t::on_write, shared_from_this(), ph::_1))
        );
    }

private:
    void
    on_write(const std::error_code& ec) {
        CF_DBG("<< write: %s", CF_EC(ec));

        if (ec) {
            session->on_error(ec);
            pr.set_exception(std::system_error(ec));
        } else {
            pr.set_value();
        }
    }
};

basic_session_t::basic_session_t(scheduler_t& scheduler) noexcept :
    scheduler(scheduler),
    closed(false),
    state(0),
    counter(1),
    message(boost::none)
{}

basic_session_t::~basic_session_t() {}

bool basic_session_t::connected() const noexcept {
    return state == static_cast<int>(state_t::connected);
}

auto basic_session_t::connect(const endpoint_type& endpoint) -> task<std::error_code>::future_type {
    return connect(std::vector<endpoint_type> {{ endpoint }});
}

framework::future<std::error_code>
basic_session_t::connect(const std::vector<endpoint_type>& endpoints) {
    CF_CTX("bC");
    CF_DBG(">> connecting ...");

    promise<std::error_code> pr;
    auto fr = pr.get_future();

    int expected(static_cast<int>(state_t::disconnected));
    const bool exchanged = state.compare_exchange_strong(expected, static_cast<int>(state_t::connecting));

    if (exchanged) {
        // The transport is disconnected, perform connecting.
        std::unique_ptr<socket_type> socket;

        try {
            socket.reset(new socket_type(scheduler.loop().loop));
        } catch (const std::exception& err) {
            CF_DBG("<< failed: %s", err.what());

            state = static_cast<int>(state_t::disconnected);
            pr.set_exception(err);
            return fr;
        }

        const auto converted = endpoints_cast<asio::ip::tcp::endpoint>(endpoints);

        socket_type& socket_ref = *socket;
        asio::async_connect(
            socket_ref,
            converted.begin(), converted.end(),
            trace::wrap(std::bind(
                &basic_session_t::on_connect,
                shared_from_this(), ph::_1, std::move(pr), std::move(socket)
            ))
        );
    } else {
        // The transport was in other state.

        switch (static_cast<state_t>(expected)) {
        case state_t::connecting:
            CF_DBG("<< already in progress");
            pr.set_value(asio::error::already_started);
            break;
        case state_t::connected:
            CF_DBG("<< already connected");
            pr.set_value(asio::error::already_connected);
            break;
        default:
            BOOST_ASSERT(false);
        }
    }

    return fr;
}

auto basic_session_t::endpoint() const -> boost::optional<endpoint_type> {
    // TODO: Implement `basic_session_t::endpoint()`.
    return boost::none;
}

basic_session_t::native_handle_type
basic_session_t::native_handle() const {
    return (*transport.synchronize())->socket->native_handle();
}

void
basic_session_t::cancel() {
    CF_DBG(">> disconnecting ...");

    closed = true;
    if (channels->empty()) {
        CF_DBG("<< stop listening");
        transport.synchronize()->reset();
    }

    CF_DBG("<< disconnected");
}

auto basic_session_t::invoke(std::function<io::encoder_t::message_type(std::uint64_t)> encoder)
    -> task<invoke_result>::future_type
{
    // Synchronization here is required to prevent channel id mixing in multi-threaded environment.
    std::lock_guard<std::mutex> lock(mutex);

    const auto span = counter++;

    CF_CTX("bI" + std::to_string(span));
    CF_DBG("invoking span %llu event ...", CF_US(span));

    auto tx = std::make_shared<basic_sender_t<basic_session_t>>(span, shared_from_this());
    auto state = std::make_shared<shared_state_t>();
    auto rx = std::make_shared<basic_receiver_t<basic_session_t>>(span, shared_from_this(), state);

    channels->insert(std::make_pair(span, std::move(state)));
    return push(encoder(span))
        .then(scheduler, trace::wrap([tx, rx](task<void>::future_move_type future) -> invoke_result {
            future.get();
            return std::make_tuple(tx, rx);
        }));
}

framework::future<void>
basic_session_t::push(io::encoder_t::message_type&& message) {
    CF_CTX("bP");
    CF_DBG(">> writing message ...");

    promise<void> pr;
    auto fr = pr.get_future();

    auto transport = *this->transport.synchronize();
    if (transport) {
        auto pusher = std::make_shared<push_t>(std::move(message), shared_from_this(), std::move(pr));
        (*pusher)(transport);
    } else {
        pr.set_exception(std::system_error(asio::error::not_connected));
    }

    return fr;
}

void basic_session_t::revoke(std::uint64_t span) {
    CF_DBG(">> revoking span %llu channel", CF_US(span));

    auto channels = this->channels.synchronize();
    channels->erase(span);
    if (closed && channels->empty()) {
        // At this moment there are no references left to this session and also nobody is intrested
        // for data reading.
        CF_DBG("<< stop listening");
        transport.synchronize()->reset();
    }

    CF_DBG("<< revoke span %llu channel", CF_US(span));
}

void
basic_session_t::on_connect(const std::error_code& ec, promise<std::error_code> pr, std::unique_ptr<socket_type>& socket) {
    CF_DBG("<< connect: %s", CF_EC(ec));

    if (ec) {
        state = static_cast<std::uint8_t>(state_t::disconnected);
        transport.synchronize()->reset();
    } else {
        CF_CTX_POP();
        CF_CTX("bR");
        CF_DBG(">> listening for read events ...");

        state = static_cast<std::uint8_t>(state_t::connected);
        auto transport = this->transport.synchronize();
        transport->reset(new channel_type(std::move(socket)));
        pull(*transport);
    }

    pr.set_value(ec);
}

void
basic_session_t::on_read(const std::error_code& ec) {
    CF_DBG("<< read: %s", CF_EC(ec));

    if (ec) {
        on_error(ec);
        return;
    }

    CF_DBG("received message [%llu, %llu, %s]", CF_US(message.span()), CF_US(message.type()), CF_MSG(message.args()).c_str());
    auto state = channels.apply([&](channels_type& channels) -> std::shared_ptr<shared_state_t> {
         auto it = channels.find(message.span());
         if (it == channels.end()) {
             CF_DBG("dropping an orphan span %llu message", CF_US(message.span()));
             return nullptr;
         } else {
             return it->second;
         }
    });

    if (state) {
        state->put(std::move(message));
    }

    auto transport = this->transport.synchronize();
    if (*transport) {
        pull(*transport);
    }
}

void
basic_session_t::on_error(const std::error_code& ec) {
    BOOST_ASSERT(ec);

    state = static_cast<std::uint8_t>(state_t::disconnected);

    auto channels = this->channels.apply([&](channels_type& channels) -> channels_type {
        auto copy = channels;
        channels.clear();
        return copy;
    });

    for (auto channel : channels) {
        channel.second->put(ec);
    }
}

void
basic_session_t::pull(std::shared_ptr<channel_type> transport) {
    CF_DBG(">> listening for read events ...");

    transport->reader->read(
        message,
        trace::wrap(std::bind(&basic_session_t::on_read, shared_from_this(), ph::_1))
    );
}

#include "sender.cpp"
template class cocaine::framework::basic_sender_t<basic_session_t>;

#include "receiver.cpp"
template class cocaine::framework::basic_receiver_t<basic_session_t>;
