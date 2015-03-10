#include "cocaine/framework/worker/sender.hpp"

#include <cocaine/detail/service/node/messages.hpp>

#include "cocaine/framework/sender.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

namespace {

worker::sender
on_write(typename task<void>::future_move_type future, std::shared_ptr<basic_sender_t<worker_session_t>> session) {
    future.get();
    return worker::sender(session);
}

void
on_error(typename task<void>::future_type& future) {
    future.get();
}

void
on_close(typename task<void>::future_type& future) {
    future.get();
}

} // namespace

worker::sender::sender(std::shared_ptr<basic_sender_t<worker_session_t>> session) :
    session(std::move(session))
{}

worker::sender::~sender() {
    // Close this stream if it wasn't explicitly closed.
    if (session) {
        close();
    }
}

auto worker::sender::write(std::string message) -> typename task<worker::sender>::future_type {
    auto session = std::move(this->session);

    return session->send<io::rpc::chunk>(std::move(message))
        .then(std::bind(&on_write, ph::_1, session));
}

auto worker::sender::error(int id, std::string reason) -> typename task<void>::future_type {
    auto session = std::move(this->session);

    return session->send<io::rpc::error>(id, std::move(reason))
        .then(std::bind(&on_error, ph::_1));
}

auto worker::sender::close() -> typename task<void>::future_type {
    auto session = std::move(this->session);

    return session->send<io::rpc::choke>()
        .then(std::bind(&on_close, ph::_1));
}
