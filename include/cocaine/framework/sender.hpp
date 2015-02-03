#pragma once

#include <cstdint>
#include <memory>

#include <cocaine/rpc/asio/encoder.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

// app::enqueue - allows [write | error | close]
//   write - -//-,
//   error - void,
//   close - void.
class basic_session_t;

class basic_sender_t {
    std::uint64_t id;
    std::shared_ptr<basic_session_t> session;

public:
    basic_sender_t(std::uint64_t id, std::shared_ptr<basic_session_t> session);
    basic_sender_t(basic_sender_t&& other) = default;

    /*!
     * \todo \throw encoding_error if the sender has failed to encode the arguments given.
     * \todo \throw std::system_error if the sender is in invalid state, for example after any network
     * error.
     */
    template<class Event, class... Args>
    auto
    send(Args&&... args) -> future_t<void> {
        return send(io::encoded<Event>(id, std::forward<Args>(args)...));
    }

private:
    auto send(io::encoder_t::message_type&& message) -> future_t<void>;
};

template<class T, class Event>
struct has_slot : public boost::mpl::contains<io::protocol<T>, Event> {};

template<class T>
class sender {
public:
    typedef T tag_type;

private:
    std::shared_ptr<basic_sender_t> d;

public:
    sender(std::uint64_t id, std::shared_ptr<basic_session_t> session) :
        d(new basic_sender_t(id, session))
    {}

    sender(std::shared_ptr<basic_sender_t> base) :
        d(std::move(base))
    {}

    /*!
     * Copy constructor is deleted intentionally.
     *
     * This is necessary, because each `send` method call in the common case invalidates current
     * object's state and returns the new sender instead.
     */
    sender(const sender& other) = delete;
    sender(sender&& other) = default;

    sender& operator=(const sender& other) = delete;
    sender& operator=(sender&& other) = default;

    template<class Event, class... Args>
//    typename std::enable_if<
//        has_slot<tag_type, Event>::value,
//        future_t<sender<typename io::event_traits<Event>::dispatch_type>>
//    >::type
    future_t<sender<typename io::event_traits<Event>::dispatch_type>>
    send(Args&&... args) {
        auto future = d->send<Event>(std::forward<Args>(args)...);
        return future.then(std::bind(&sender::traverse<Event>, std::placeholders::_1, d));
    }

private:
    template<class Event>
    static
    sender<typename io::event_traits<Event>::dispatch_type>
    traverse(future_t<void>& f, std::shared_ptr<basic_sender_t> s) {
        f.get();
        return sender<typename io::event_traits<Event>::dispatch_type>(std::move(s));
    }
};

template<>
class sender<void> {
public:
    sender(std::uint64_t, std::shared_ptr<basic_session_t>) {}
    sender(std::shared_ptr<basic_sender_t>) {}
};

} // namespace framework

} // namespace cocaine
