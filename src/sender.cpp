#include <cocaine/common.hpp>

#include "cocaine/framework/sender.hpp"
#include "cocaine/framework/session.hpp"

#include "cocaine/framework/detail/basic_session.hpp"

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
