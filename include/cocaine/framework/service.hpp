#pragma once

#include <boost/asio/ip/tcp.hpp>

#include <cocaine/common.hpp>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/session.hpp"
#include "cocaine/framework/receiver.hpp"
#include "cocaine/framework/scheduler.hpp"

namespace cocaine {

namespace framework {

template<
    class Event,
    class Upstream = typename io::event_traits<Event>::upstream_type,
    class Dispatch = typename io::event_traits<Event>::dispatch_type
>
struct invocation_result;

//! \note value or throw.
template<class Event, class T>
struct invocation_result<Event, io::primitive_tag<T>, void> {
    typedef typename detail::packable<T>::type value_type;
    typedef std::tuple<sender<void, basic_session_t>, receiver<io::primitive_tag<T>, basic_session_t>> channel_type;
    typedef value_type type;

    static
    future_type<value_type>
    apply(channel_type&& channel) {
        auto rx = std::move(std::get<1>(channel));
        return rx.recv();
    }
};

//! \note sender - usual, receiver - special - recv() -> optional<T> or throw.
template<class Event, class U, class D>
struct invocation_result<Event, io::streaming_tag<U>, io::streaming_tag<D>> {
    typedef typename detail::packable<U>::type value_type;
    typedef std::tuple<sender<io::streaming_tag<D>, basic_session_t>, receiver<io::streaming_tag<U>, basic_session_t>> channel_type;
    typedef channel_type type;

    static
    future_type<type>
    apply(channel_type&& channel) {
        return make_ready_future<type>::value(std::move(channel));
    }
};

class basic_service_t {
    class impl;
    std::unique_ptr<impl> d;
    std::shared_ptr<session<>> sess;
    scheduler_t& scheduler;

public:
    basic_service_t(std::string name, uint version, scheduler_t& scheduler);
    ~basic_service_t();

    auto name() const noexcept -> const std::string&;
    auto version() const noexcept -> uint;

    auto connect() -> future_type<void>;

    template<class Event, class... Args>
    future_type<typename invocation_result<Event>::type>
    invoke(Args&&... args) {
        namespace ph = std::placeholders;

        return connect()
            .then(scheduler, std::bind(&basic_service_t::on_connect<Event, Args...>, ph::_1, sess, std::forward<Args>(args)...))
            .then(scheduler, std::bind(&basic_service_t::on_invoke<Event>, ph::_1));
    }

private:
    template<class Event, class... Args>
    static
    future_type<typename session<>::invoke_result<Event>::type>
    on_connect(future_type<void>& f, std::shared_ptr<session<>> sess, Args&... args) {
        f.get();
        // Between these calls no one can guarantee, that the connection won't be broken. In this
        // case you will get a system error after either write or read attempt.
        return sess->invoke<Event>(std::forward<Args>(args)...);
    }

    template<class Event>
    static
    future_type<typename invocation_result<Event>::type>
    on_invoke(future_type<typename session<>::invoke_result<Event>::type>& f) {
        return invocation_result<Event>::apply(f.get());
    }
};

} // namespace framework

} // namespace cocaine
