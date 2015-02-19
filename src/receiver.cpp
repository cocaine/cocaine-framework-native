#include "cocaine/framework/receiver.hpp"

#include "cocaine/framework/session.hpp"

using namespace cocaine::framework;

template<class Session>
basic_receiver_t<Session>::basic_receiver_t(std::uint64_t id, std::shared_ptr<Session> session, std::shared_ptr<detail::shared_state_t> state) :
    id(id),
    session(session),
    state(state)
{}

template<class Session>
basic_receiver_t<Session>::~basic_receiver_t() {
    // TODO: Really need?
    session->revoke(id);
}

template<class Session>
typename task<typename basic_receiver_t<Session>::result_type>::future_type
basic_receiver_t<Session>::recv() {
    return state->get();
}

template<class Session>
void basic_receiver_t<Session>::revoke() {
//    session->revoke(id);
}

template class cocaine::framework::basic_receiver_t<basic_session_t>;

#include "cocaine/framework/worker.hpp"
template class cocaine::framework::basic_receiver_t<worker_session_t>;
