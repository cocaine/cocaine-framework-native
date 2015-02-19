#include "cocaine/framework/worker/sender.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

typedef worker_session_t session_type;
typedef basic_sender_t<session_type> basic_session_type;
typedef sender<io::rpc_tag, session_type> sender_type;

sender_type::sender(std::shared_ptr<basic_session_type> session) :
    session(session)
{}

sender_type::~sender() {
    // Close this stream if it wasn't explicitly closed.
    if (session) {
        close();
    }
}

namespace {

sender_type
on_write(typename task<void>::future_type& f, std::shared_ptr<basic_session_type> session) {
    f.get();
    return sender_type(session);
}

void
on_error(typename task<void>::future_type& f) {
    f.get();
}

void
on_close(typename task<void>::future_type& f) {
    f.get();
}

} // namespace

auto sender_type::write(std::string message) -> typename task<sender>::future_type {
    auto session = std::move(this->session);

    return session->template send<io::rpc::chunk>(std::move(message))
        .then(std::bind(&on_write, ph::_1, session));
}

auto sender_type::error(int id, std::string reason) -> typename task<void>::future_type {
    auto session = std::move(this->session);

    return session->template send<io::rpc::error>(id, std::move(reason))
        .then(std::bind(&on_error, ph::_1));
}

auto sender_type::close() -> typename task<void>::future_type {
    auto session = std::move(this->session);

    return session->template send<io::rpc::choke>()
        .then(std::bind(&on_close, ph::_1));
}
