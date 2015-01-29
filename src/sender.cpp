#include <cocaine/common.hpp>

#include "cocaine/framework/sender.hpp"
#include "cocaine/framework/session.hpp"

using namespace cocaine::framework;

basic_sender_t::basic_sender_t(std::uint64_t id, std::shared_ptr<basic_session_t> connection) :
    id(id),
    connection(connection)
{}

future_t<void>
basic_sender_t::send(io::encoder_t::message_type&& message) {
    return connection->push(id, std::move(message));
}
