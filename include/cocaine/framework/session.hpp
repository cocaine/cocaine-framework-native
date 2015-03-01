#pragma once

#include <cstdint>

#include <boost/asio/ip/tcp.hpp>

#include <cocaine/common.hpp>
#include <cocaine/rpc/asio/channel.hpp>

#include "cocaine/framework/config.hpp"
#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/receiver.hpp"
#include "cocaine/framework/sender.hpp"

namespace cocaine {

namespace framework {

/*!
 * RAII class that manages with connection queue and returns a typed sender/receiver.
 */
template<class BasicSession = basic_session_t>
class session {
public:
    typedef BasicSession basic_session_type;
    typedef boost::asio::ip::tcp::endpoint endpoint_type;

    typedef std::tuple<
        std::shared_ptr<basic_sender_t<basic_session_t>>,
        std::shared_ptr<basic_receiver_t<basic_session_t>>
    > basic_invoke_result;

    template<class Event>
    struct invoke_result {
        typedef sender  <typename io::event_traits<Event>::dispatch_type, basic_session_t> sender_type;
        typedef receiver<typename io::event_traits<Event>::upstream_type, basic_session_t> receiver_type;
        typedef std::tuple<sender_type, receiver_type> type;
    };

private:
    class impl;
    std::shared_ptr<impl> d;
    scheduler_t& scheduler;

public:
    explicit session(scheduler_t& scheduler);
    ~session();

    bool connected() const;

    auto connect(const endpoint_type& endpoint) -> typename task<void>::future_type;
    auto connect(const std::vector<endpoint_type>& endpoints) -> typename task<void>::future_type;

    void disconnect();

    template<class Event, class... Args>
    typename task<typename invoke_result<Event>::type>::future_type
    invoke(Args&&... args) {
        const std::uint64_t span(next());
        return invoke(span, io::encoded<Event>(span, std::forward<Args>(args)...))
            .then(scheduler, std::bind(&session::on_invoke<Event>, std::placeholders::_1));
    }

private:
    auto next() -> std::uint64_t;

    auto invoke(std::uint64_t span, io::encoder_t::message_type&& message) -> typename task<basic_invoke_result>::future_type;

    template<class Event>
    static
    typename invoke_result<Event>::type
    on_invoke(typename task<basic_invoke_result>::future_move_type future) {
        return typename invoke_result<Event>::type(future.get());
    }
};

} // namespace framework

} // namespace cocaine
