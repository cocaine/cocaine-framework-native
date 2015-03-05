#include "cocaine/framework/worker/receiver.hpp"

#include <cocaine/traits/tuple.hpp>

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

typedef worker_session_t session_type;
typedef basic_receiver_t<worker_session_t> basic_receiver_type;
typedef receiver<io::rpc_tag, worker_session_t> receiver_type;

receiver_type::receiver(std::shared_ptr<basic_receiver_type> session) :
    session(std::move(session))
{}

namespace {

boost::optional<std::string>
on_recv(typename task<decoded_message>::future_type& f) {
    const auto message = f.get();
    const std::uint64_t id = message.type();
    switch (id) {
    case io::event_traits<io::rpc::chunk>::id: {
        std::string chunk;
        io::type_traits<
            typename io::event_traits<io::rpc::chunk>::argument_type
        >::unpack(message.args(), chunk);
        return chunk;
    }
    case io::event_traits<io::rpc::error>::id: {
        int id;
        std::string reason;
        io::type_traits<
            typename io::event_traits<io::rpc::error>::argument_type
        >::unpack(message.args(), id, reason);
        throw std::runtime_error(reason);
    }
    case io::event_traits<io::rpc::choke>::id:
        return boost::none;
    default:
        COCAINE_ASSERT(false);
    }

    return boost::none;
}

} // namespace

auto receiver_type::recv() -> typename task<boost::optional<std::string>>::future_type {
    return session->recv()
        .then(std::bind(&on_recv, ph::_1));
}
