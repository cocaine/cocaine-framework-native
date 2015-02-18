#pragma once

#include <boost/asio/ip/tcp.hpp>

#include <cocaine/common.hpp>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/session.hpp"
#include "cocaine/framework/receiver.hpp"

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
    future_t<value_type>
    apply(channel_type& channel) {
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
    future_t<type>
    apply(channel_type& channel) {
        return make_ready_future<type>::value(std::move(channel));
    }
};

class basic_service_t {
    class impl;
    std::unique_ptr<impl> d;
    std::shared_ptr<session<>> sess;

public:
    basic_service_t(std::string name, uint version, scheduler_t& scheduler);
    ~basic_service_t();

    auto name() const -> const std::string&;

    auto connect() -> future_t<void>;

    template<class Event, class... Args>
    future_t<typename invocation_result<Event>::type>
    invoke(Args&&... args) {
        typedef typename invocation_result<Event>::type result_type;

        try {
            // TODO: Make asyncronous call through `then`.
            connect().get();

            auto ch = sess->invoke<Event>(std::forward<Args>(args)...).get();
            return invocation_result<Event>::apply(ch);
        } catch (const std::exception& err) {
            return make_ready_future<result_type>::error(err);
        }
    }
};

} // namespace framework

} // namespace cocaine
