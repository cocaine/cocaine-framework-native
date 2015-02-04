#include "cocaine/framework/receiver.hpp"

#include "cocaine/framework/session.hpp"

using namespace cocaine::framework;

basic_receiver_t::basic_receiver_t(std::uint64_t id, std::shared_ptr<basic_session_t> session, std::shared_ptr<detail::shared_state_t> state) :
    id(id),
    session(session),
    state(state)
{}

basic_receiver_t::~basic_receiver_t() {
    // TODO: Need?
    session->revoke(id);
}

future_t<basic_receiver_t::result_type> basic_receiver_t::recv() {
    return state->get();
}

void basic_receiver_t::revoke() {
//    session->revoke(id);
}
