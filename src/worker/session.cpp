#include "cocaine/framework/detail/worker/session.hpp"

#include <cocaine/traits/enum.hpp>

#include "cocaine/framework/scheduler.hpp"
#include "cocaine/framework/worker/error.hpp"

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/detail/loop.hpp"
#include "cocaine/framework/detail/shared_state.hpp"

namespace ph = std::placeholders;

using namespace cocaine::framework;
using namespace cocaine::framework::worker;

const std::uint64_t CONTROL_CHANNEL_ID = 1;

// TODO: Maybe make configurable?
const boost::posix_time::time_duration HEARTBEAT_TIMEOUT = boost::posix_time::seconds(10);
const boost::posix_time::time_duration DISOWN_TIMEOUT = boost::posix_time::seconds(60);

//! \note single shot.
template<class Session>
class worker_session_t::push_t : public std::enable_shared_from_this<push_t<Session>> {
    io::encoder_t::message_type message;
    std::shared_ptr<Session> session;
    typename task<void>::promise_type h;

public:
    explicit push_t(io::encoder_t::message_type&& message, std::shared_ptr<Session> session, typename task<void>::promise_type&& h) :
        message(std::move(message)),
        session(session),
        h(std::move(h))
    {}

    void operator()() {
        if (session->channel) {
            session->channel->writer->write(message, std::bind(&push_t::on_write, this->shared_from_this(), ph::_1));
        } else {
            h.set_exception(std::system_error(asio::error::not_connected));
        }
    }

private:
    void on_write(const std::error_code& ec) {
        CF_DBG("write event: %s", CF_EC(ec));

        if (ec) {
            session->on_error(ec);
            h.set_exception(std::system_error(ec));
        } else {
            h.set_value();
        }
    }
};

worker_session_t::worker_session_t(dispatch_t& dispatch, scheduler_t& scheduler, executor_t executor) :
    dispatch(dispatch),
    scheduler(scheduler),
    executor(std::move(executor)),
    message(boost::none),
    heartbeat_timer(scheduler.loop().loop),
    disown_timer(scheduler.loop().loop)
{}

void worker_session_t::connect(std::string endpoint, std::string uuid) {
    std::unique_ptr<protocol_type::socket> socket(new protocol_type::socket(scheduler.loop().loop));
    socket->connect(protocol_type::endpoint(endpoint));

    channel.reset(new channel_type(std::move(socket)));

    handshake(uuid);
    inhale();
    exhale();

    channel->reader->read(message, std::bind(&worker_session_t::on_read, shared_from_this(), ph::_1));
}

void worker_session_t::handshake(const std::string& uuid) {
    CF_DBG("<- Handshake");

    push(io::encoded<io::rpc::handshake>(CONTROL_CHANNEL_ID, uuid));
}

void worker_session_t::terminate(io::rpc::terminate::code code, std::string reason) {
    CF_DBG("<- Terminate [%d, %s]", code, reason.c_str());

    push(io::encoded<io::rpc::terminate>(CONTROL_CHANNEL_ID, code, std::move(reason)))
        .then(std::bind(&worker_session_t::on_terminate, shared_from_this(), ph::_1));
}

void worker_session_t::on_terminate(task<void>::future_move_type) {
    throw termination_error();
}

void worker_session_t::inhale() {
    disown_timer.expires_from_now(DISOWN_TIMEOUT);
    disown_timer.async_wait(std::bind(&worker_session_t::on_disown, shared_from_this(), ph::_1));
}

void worker_session_t::exhale(const std::error_code& ec) {
    if (ec) {
        COCAINE_ASSERT(ec == asio::error::operation_aborted);

        // Heartbeat timer can only be interrupted during graceful shutdown.
        return;
    }

    CF_DBG("<- ♥");

    push(io::encoded<io::rpc::heartbeat>(CONTROL_CHANNEL_ID));

    heartbeat_timer.expires_from_now(HEARTBEAT_TIMEOUT);
    heartbeat_timer.async_wait(std::bind(&worker_session_t::exhale, shared_from_this(), ph::_1));
}

void worker_session_t::on_disown(const std::error_code& ec) {
    if (ec) {
        COCAINE_ASSERT(ec == asio::error::operation_aborted);

        // Do nothing, because it's just usual timer reset.
        return;
    }

    on_error(asio::error::make_error_code(asio::error::timed_out));

    throw disowned_error(DISOWN_TIMEOUT.seconds());
}

auto worker_session_t::push(std::uint64_t span, io::encoder_t::message_type&& message) -> typename task<void>::future_type {
    auto channels = this->channels.synchronize();
    auto it = channels->find(span);
    if (it == channels->end()) {
        // TODO: Consider is it ever possible?
        return make_ready_future<void>::error(
            std::runtime_error("trying to send message through non-registered channel")
        );
    }

    return push(std::move(message));
}

auto worker_session_t::push(io::encoder_t::message_type&& message) -> typename task<void>::future_type {
    typename task<void>::promise_type promise;
    auto future = promise.get_future();

    scheduler(std::bind(&push_t<worker_session_t>::operator(),
                        std::make_shared<push_t<worker_session_t>>(std::move(message), shared_from_this(), std::move(promise))));
    return future;
}

void worker_session_t::on_read(const std::error_code& ec) {
    CF_DBG("read event: %s", CF_EC(ec));

    if (ec) {
        on_error(ec);
        return;
    }

    process();

    CF_DBG("waiting for more data ...");
    channel->reader->read(message, std::bind(&worker_session_t::on_read, this, ph::_1));
}

void worker_session_t::on_error(const std::error_code&) {
    // Error (broken network for example).
    // TODO: disconnect all channels with that error.
}

void worker_session_t::revoke(std::uint64_t span) {
    CF_DBG("revoking span %llu channel", CF_US(span));

    channels->erase(span);
}

void worker_session_t::process() {
    CF_DBG("event %llu, span %llu", CF_US(message.type()), CF_US(message.span()));

    const auto id = message.type();
    switch (id) {
    case (io::event_traits<io::rpc::handshake>::id):
        process_handshake();
        break;
    case (io::event_traits<io::rpc::heartbeat>::id):
        process_heartbeat();
        break;
    case (io::event_traits<io::rpc::terminate>::id):
        process_terminate();
        break;
    case (io::event_traits<io::rpc::invoke>::id):
        process_invoke();
        break;
    case (io::event_traits<io::rpc::chunk>::id):
        process_chunk();
        break;
    case (io::event_traits<io::rpc::error>::id):
        process_error();
        break;
    case (io::event_traits<io::rpc::choke>::id):
        process_choke();
        break;
    default:
        throw invalid_protocol_type(id);
    }
}

void worker_session_t::process_handshake() {
    CF_DBG("-> Handshake");
    CF_WRN("invalid protocol: the runtime should never send handshake event");
}

void worker_session_t::process_heartbeat() {
    // We received a heartbeat message from the runtime. Reset the disown timer.
    CF_DBG("-> ♥");

    inhale();
}

void worker_session_t::process_terminate() {
    CF_DBG("-> Terminate");
    terminate(io::rpc::terminate::normal, "confirmed");
}

void worker_session_t::process_invoke() {
    std::string event;
    io::type_traits<
        typename io::event_traits<io::rpc::invoke>::argument_type
    >::unpack(message.args(), event);
    CF_DBG("-> Invoke '%s'", event.c_str());

    auto handler = dispatch.get(event);
    if (!handler) {
        CF_DBG("event '%s' not found", event.c_str());
        push(io::encoded<io::rpc::error>(message.span(), 1, "event '" + event + "' not found"));
        return;
    }

    auto id = message.span();
    auto tx = std::make_shared<basic_sender_t<worker_session_t>>(id, shared_from_this());
    auto state = std::make_shared<shared_state_t>();
    auto rx = std::make_shared<basic_receiver_t<worker_session_t>>(id, shared_from_this(), state);

    channels->insert(std::make_pair(id, state));
    executor([handler, tx, rx](){
        (*handler)(tx, rx);
    });
}

void worker_session_t::process_chunk() {
    auto channels = this->channels.synchronize();
    auto it = channels->find(message.span());
    if (it == channels->end()) {
        CF_DBG("received an orphan span %llu type %llu message", CF_US(message.span()), CF_US(message.type()));
        return;
    }

    it->second->put(std::move(message));
}

void worker_session_t::process_error() {
    auto channels = this->channels.synchronize();
    auto it = channels->find(message.span());
    if (it == channels->end()) {
        CF_DBG("received an orphan span %llu type %llu message", CF_US(message.span()), CF_US(message.type()));
        return;
    }

    it->second->put(std::move(message));
    channels->erase(it);
}

void worker_session_t::process_choke() {
    process_error();
}

#include "../sender.cpp"
#include "../receiver.cpp"

template class cocaine::framework::basic_sender_t<worker_session_t>;
template class cocaine::framework::basic_receiver_t<worker_session_t>;
