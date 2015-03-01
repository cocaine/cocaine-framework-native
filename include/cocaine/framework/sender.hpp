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
     * Pack given args in the message and push it through session pointer.
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
        auto d = std::move(this->d);
        auto future = d->template send<Event>(std::forward<Args>(args)...);
        return future.then(std::bind(&sender::traverse<Event>, std::placeholders::_1, std::move(d)));
    }

private:
    template<class Event>
    static
    sender<typename io::event_traits<Event>::dispatch_type, Session>
    traverse(typename task<void>::future_type& future, std::shared_ptr<basic_sender_t<Session>> d) {
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
