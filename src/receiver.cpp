#include "cocaine/framework/receiver.hpp"

#include "cocaine/framework/session.hpp"

using namespace cocaine::framework;

basic_receiver_t::basic_receiver_t(std::uint64_t id, std::shared_ptr<basic_session_t> session) :
    id(id),
    session(session)
{}

basic_receiver_t::~basic_receiver_t() {
    session->revoke(id);
}
