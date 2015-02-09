#include <cocaine/common.hpp>

#include "cocaine/framework/sender.hpp"
#include "cocaine/framework/session.hpp"

using namespace cocaine::framework;

template<class Pusher>
basic_sender_t<Pusher>::basic_sender_t(std::uint64_t id, std::shared_ptr<pusher_type> session) :
    id(id),
    session(session)
{}

template<class Pusher>
future_t<void>
basic_sender_t<Pusher>::send(io::encoder_t::message_type&& message) {
    return session->push(id, std::move(message));
}

template class cocaine::framework::basic_sender_t<basic_session_t>;

#include "cocaine/framework/worker.hpp"
template class cocaine::framework::basic_sender_t<worker_session_t>;
