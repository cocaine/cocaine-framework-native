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

#include "cocaine/framework/detail/worker/session.hpp"

#include <cocaine/traits/enum.hpp>
#include <cocaine/idl/streaming.hpp>

#include "cocaine/framework/scheduler.hpp"
#include "cocaine/framework/worker/error.hpp"

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/detail/loop.hpp"
#include "cocaine/framework/detail/shared_state.hpp"

namespace ph = std::placeholders;

using namespace cocaine::framework;
using namespace cocaine::framework::worker;

const std::uint64_t CONTROL_CHANNEL_ID = 1;

// TODO: Maybe make configurable?
const boost::posix_time::time_duration HEARTBEAT_TIMEOUT = boost::posix_time::seconds(10);
const boost::posix_time::time_duration DISOWN_TIMEOUT = boost::posix_time::seconds(60);

//! \note single shot.
template<class Session>
class worker_session_t::push_t : public std::enable_shared_from_this<push_t<Session>> {
    io::encoder_t::message_type message;
    std::shared_ptr<Session> session;
    task<void>::promise_type h;

public:
    explicit push_t(io::encoder_t::message_type&& message, std::shared_ptr<Session> session, task<void>::promise_type&& h) :
        message(std::move(message)),
        session(session),
        h(std::move(h))
    {}

    void operator()() {
        if (session->channel) {
            session->channel->writer->write(message, std::bind(&push_t::on_write, this->shared_from_this(), ph::_1));
        } else {
            h.set_exception(std::system_error(asio::error::not_connected));
        }
    }

private:
    void on_write(const std::error_code& ec) {
        CF_DBG("write event: %s", CF_EC(ec));

        if (ec) {
            session->on_error(ec);
            h.set_exception(std::system_error(ec));
        } else {
            h.set_value();
        }
    }
};

worker_session_t::worker_session_t(dispatch_t& dispatch, scheduler_t& scheduler, executor_t executor) :
    dispatch(dispatch),
    scheduler(scheduler),
    executor(std::move(executor)),
    message(boost::none),
    heartbeat_timer(scheduler.loop().loop),
    disown_timer(scheduler.loop().loop)
{}

void worker_session_t::connect(std::string endpoint, std::string uuid) {
    std::unique_ptr<protocol_type::socket> socket(new protocol_type::socket(scheduler.loop().loop));
    socket->connect(protocol_type::endpoint(endpoint));

    channel.reset(new channel_type(std::move(socket)));

    handshake(uuid);
    inhale();
    exhale();

    channel->reader->read(message, std::bind(&worker_session_t::on_read, shared_from_this(), ph::_1));
}

void worker_session_t::handshake(const std::string& uuid) {
    CF_DBG("<- Handshake");

    push(io::encoded<io::worker::handshake>(CONTROL_CHANNEL_ID, uuid));
}

void worker_session_t::terminate(int code, std::string reason) {
    CF_DBG("<- Terminate [%d, %s]", code, reason.c_str());

    push(io::encoded<io::worker::terminate>(CONTROL_CHANNEL_ID, code, std::move(reason)))
        .then(std::bind(&worker_session_t::on_terminate, shared_from_this(), ph::_1));
}

void worker_session_t::on_terminate(task<void>::future_move_type) {
    throw termination_error();
}

void worker_session_t::inhale() {
    disown_timer.expires_from_now(DISOWN_TIMEOUT);
    disown_timer.async_wait(std::bind(&worker_session_t::on_disown, shared_from_this(), ph::_1));
}

void worker_session_t::exhale(const std::error_code& ec) {
    if (ec) {
        BOOST_ASSERT(ec == asio::error::operation_aborted);

        // Heartbeat timer can only be interrupted during graceful shutdown.
        return;
    }

    CF_DBG("<- ♥");

    push(io::encoded<io::worker::heartbeat>(CONTROL_CHANNEL_ID));

    heartbeat_timer.expires_from_now(HEARTBEAT_TIMEOUT);
    heartbeat_timer.async_wait(std::bind(&worker_session_t::exhale, shared_from_this(), ph::_1));
}

void worker_session_t::on_disown(const std::error_code& ec) {
    if (ec) {
        BOOST_ASSERT(ec == asio::error::operation_aborted);

        // Do nothing, because it's just usual timer reset.
        return;
    }

    on_error(worker::error::disowned);

    throw disowned_error(DISOWN_TIMEOUT.seconds());
}

auto worker_session_t::push(std::uint64_t span, io::encoder_t::message_type&& message) -> task<void>::future_type {
    auto channels = this->channels.synchronize();
    auto it = channels->find(span);
    if (it == channels->end()) {
        return make_ready_future<void>::error(
            std::runtime_error("trying to send message through non-registered channel")
        );
    }

    return push(std::move(message));
}

auto worker_session_t::push(io::encoder_t::message_type&& message) -> task<void>::future_type {
    task<void>::promise_type promise;
    auto future = promise.get_future();

    scheduler(std::bind(&push_t<worker_session_t>::operator(),
                        std::make_shared<push_t<worker_session_t>>(std::move(message), shared_from_this(), std::move(promise))));
    return future;
}

void worker_session_t::on_read(const std::error_code& ec) {
    CF_DBG("read event: %s", CF_EC(ec));

    if (ec) {
        on_error(ec);
        return;
    }

    process();

    CF_DBG("waiting for more data ...");
    channel->reader->read(message, std::bind(&worker_session_t::on_read, this, ph::_1));
}

void worker_session_t::on_error(const std::error_code& ec) {
    CF_DBG("on error: %s", CF_EC(ec));
    BOOST_ASSERT(ec);

    auto channels = this->channels.synchronize();
    for (auto channel : *channels) {
        channel.second->put(ec);
    }
    channels->clear();
}

void worker_session_t::revoke(std::uint64_t span) {
    CF_DBG("revoking span %llu channel", CF_US(span));

    channels->erase(span);
}

void worker_session_t::process() {
    CF_DBG("event %llu, span %llu", CF_US(message.type()), CF_US(message.span()));

    const auto id   = message.type();
    const auto span = message.span();

    if (span == 0) {
        CF_DBG("dropping 0 channel message - the specified channel number is forbidden");
    } else if (span == 1) {
        switch (id) {
        case (io::event_traits<io::worker::heartbeat>::id):
            process_heartbeat();
            break;
        case (io::event_traits<io::worker::terminate>::id):
            process_terminate();
            break;
        default:
            throw invalid_protocol_type(id);
        }
    } else {
        std::map<std::uint64_t, std::shared_ptr<shared_state_t>>::const_iterator lb, ub;
        channels.apply([&](std::map<std::uint64_t, std::shared_ptr<shared_state_t>>& channels) {
            std::tie(lb, ub) = channels.equal_range(span);
            if (lb == channels.end() && ub == channels.end()) {
                if (id == io::event_traits<io::worker::rpc::invoke>::id) {
                    process_invoke(channels);
                } else {
                    throw invalid_protocol_type(id);
                }
            } else if (lb == ub) {
                CF_DBG("dropping %d channel message - the specified channel was revoked", CF_US(span));
                return;
            } else {
                typedef io::protocol<io::worker::rpc::invoke::upstream_type>::scope protocol;

                switch (id) {
                case (io::event_traits<protocol::chunk>::id):
                    lb->second->put(std::move(message));
                    break;
                case (io::event_traits<protocol::error>::id):
                    lb->second->put(std::move(message));
                    channels.erase(lb);
                    break;
                case (io::event_traits<protocol::choke>::id):
                    lb->second->put(std::move(message));
                    channels.erase(lb);
                    break;
                default:
                    throw invalid_protocol_type(id);
                }
            }
        });
    }
}

void worker_session_t::process_heartbeat() {
    // We received a heartbeat message from the runtime. Reset the disown timer.
    CF_DBG("-> ♥");

    inhale();
}

void worker_session_t::process_terminate() {
    CF_DBG("-> Terminate");
    terminate(0, "confirmed");
}

void worker_session_t::process_invoke(std::map<std::uint64_t, std::shared_ptr<shared_state_t>>& channels) {
    std::string event;
    io::type_traits<
        io::event_traits<io::worker::rpc::invoke>::argument_type
    >::unpack(message.args(), event);
    CF_DBG("-> Invoke '%s'", event.c_str());

    auto handler = dispatch.get(event);
    if (!handler) {
        CF_DBG("event '%s' not found", event.c_str());
        typedef io::protocol<io::worker::rpc::invoke::upstream_type>::scope protocol;
        push(io::encoded<protocol::error>(message.span(), 1, "event '" + event + "' not found"));
        return;
    }

    auto id = message.span();
    auto tx = std::make_shared<basic_sender_t<worker_session_t>>(id, shared_from_this());
    auto state = std::make_shared<shared_state_t>();
    auto rx = std::make_shared<basic_receiver_t<worker_session_t>>(id, shared_from_this(), state);

    channels.insert(std::make_pair(id, state));
    executor([handler, tx, rx](){
        (*handler)(tx, rx);
    });
}

#include "../sender.cpp"
#include "../receiver.cpp"

template class cocaine::framework::basic_sender_t<worker_session_t>;
template class cocaine::framework::basic_receiver_t<worker_session_t>;
