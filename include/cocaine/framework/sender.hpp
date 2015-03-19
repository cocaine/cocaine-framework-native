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

#include <cocaine/rpc/asio/encoder.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

template<class Session>
class basic_sender_t {
    typedef Session session_type;

    std::uint64_t id;
    std::shared_ptr<session_type> session;

public:
    basic_sender_t(std::uint64_t id, std::shared_ptr<session_type> session);

    /*!
     * Pack given args in the message and push it through a session pointer.
     *
     * \throw a context-specific exception if the sender is unable to properly pack the arguments
     * given.
     *
     * \return a future, which will be set when the message is completely sent or any network error
     * occured.
     * This future can throw std::system_error if the sender is in invalid state, for example after
     * any network error.
     * Setting an error in the receiver doesn't work, because there can be mute events, which
     * doesn't respond ever.
     */
    template<class Event, class... Args>
    auto
    send(Args&&... args) -> typename task<void>::future_type {
        return send(io::encoded<Event>(id, std::forward<Args>(args)...));
    }

private:
    auto send(io::encoder_t::message_type&& message) -> typename task<void>::future_type;
};

template<class T, class Session>
class sender {
public:
    typedef T tag_type;

private:
    std::shared_ptr<basic_sender_t<Session>> d;

public:
    sender(std::shared_ptr<basic_sender_t<Session>> d) :
        d(std::move(d))
    {}

    /*!
     * Copy constructor is deleted intentionally.
     *
     * This is necessary, because each `send` method call in the common case invalidates current
     * object's state and returns the new sender instead.
     */
    sender(const sender& other) = delete;
    sender(sender&&) = default;

    sender& operator=(const sender& other) = delete;
    sender& operator=(sender&&) = default;

    /*!
     * Encode arguments to the internal protocol message and push it into the session attached.
     *
     * \warning this sender will be invalidated after this call.
     */
    template<class Event, class... Args>
    typename task<sender<typename io::event_traits<Event>::dispatch_type, Session>>::future_type
    send(Args&&... args) {
        BOOST_ASSERT(this->d);

        auto d = std::move(this->d);
        auto future = d->template send<Event>(std::forward<Args>(args)...);
        return future.then(std::bind(&sender::traverse<Event>, std::placeholders::_1, std::move(d)));
    }

private:
    template<class Event>
    static
    sender<typename io::event_traits<Event>::dispatch_type, Session>
    traverse(task<void>::future_move_type future, std::shared_ptr<basic_sender_t<Session>> d) {
        future.get();
        return sender<typename io::event_traits<Event>::dispatch_type, Session>(std::move(d));
    }
};

template<class Session>
class sender<void, Session> {
public:
    sender(std::shared_ptr<basic_sender_t<Session>>) {}
};

} // namespace framework

} // namespace cocaine
