#include <cocaine/common.hpp>

#include "cocaine/framework/sender.hpp"
#include "cocaine/framework/session.hpp"

using namespace cocaine::framework;

template<class Session>
basic_sender_t<Session>::basic_sender_t(std::uint64_t id, std::shared_ptr<session_type> session) :
    id(id),
    session(session)
{}

template<class Session>
typename task<void>::future_type
basic_sender_t<Session>::send(io::encoder_t::message_type&& message) {
    return session->push(std::move(message));
}

template class cocaine::framework::basic_sender_t<basic_session_t>;

#include "cocaine/framework/worker.hpp"
template class cocaine::framework::basic_sender_t<worker_session_t>;
