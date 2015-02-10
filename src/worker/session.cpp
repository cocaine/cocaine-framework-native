#include "cocaine/framework/worker.hpp"

namespace ph = std::placeholders;

using namespace cocaine::framework;

const std::uint64_t CONTROL_CHANNEL = 1;

// TODO: Maybe make configurable?
const boost::posix_time::time_duration HEARTBEAT_TIMEOUT = boost::posix_time::seconds(10);
const boost::posix_time::time_duration DISOWN_TIMEOUT = boost::posix_time::seconds(60);

//! \note single shot.
template<class Connection>
class worker_session_t::push_t : public std::enable_shared_from_this<push_t<Connection>> {
    io::encoder_t::message_type message;
    std::shared_ptr<Connection> connection;
    promise_t<void> h;

public:
    explicit push_t(io::encoder_t::message_type&& message, std::shared_ptr<Connection> connection, promise_t<void>&& h) :
        message(std::move(message)),
        connection(connection),
        h(std::move(h))
    {}

    void operator()() {
        if (connection->channel) {
            connection->channel->writer->write(message, std::bind(&push_t::on_write, this->shared_from_this(), ph::_1));
        } else {
            h.set_exception(std::system_error(asio::error::not_connected));
        }
    }

private:
    void on_write(const std::error_code& ec) {
        CF_DBG("write event: %s", CF_EC(ec));

        if (ec) {
            connection->on_error(ec);
            h.set_exception(std::system_error(ec));
        } else {
            h.set_value();
        }
    }
};

worker_session_t::worker_session_t(loop_t& loop, dispatch_t& dispatch) :
    loop(loop),
    dispatch(dispatch),
    heartbeat_timer(loop),
    disown_timer(loop)
{}

void worker_session_t::connect(std::string endpoint, std::string uuid) {
    std::unique_ptr<socket_type> socket(new socket_type(loop));
    socket->connect(protocol_type::endpoint(endpoint));

    channel.reset(new channel_type(std::move(socket)));

    handshake(uuid);

    disown_timer.expires_from_now(DISOWN_TIMEOUT);
    disown_timer.async_wait(std::bind(&worker_session_t::terminate, shared_from_this(), ph::_1));

    exhale();

    channel->reader->read(message, std::bind(&worker_session_t::on_read, shared_from_this(), ph::_1));
}

void worker_session_t::handshake(const std::string& uuid) {
    CF_DBG("<- Handshake");

    push(io::encoded<io::rpc::handshake>(CONTROL_CHANNEL, uuid));
}

void worker_session_t::inhale() {
    CF_DBG("-> ♥");

    disown_timer.expires_from_now(DISOWN_TIMEOUT);
    disown_timer.async_wait(std::bind(&worker_session_t::terminate, shared_from_this(), ph::_1));
}

void worker_session_t::exhale(const std::error_code& ec) {
    if (ec) {
        // TODO: Log.
        // TODO: Stop the session.
        return;
    }

    CF_DBG("<- ♥");

    push(io::encoded<io::rpc::heartbeat>(CONTROL_CHANNEL));

    heartbeat_timer.expires_from_now(HEARTBEAT_TIMEOUT);
    heartbeat_timer.async_wait(std::bind(&worker_session_t::exhale, shared_from_this(), ph::_1));
}

void worker_session_t::terminate(const std::error_code& ec) {
    if (ec) {
        if (ec == asio::error::operation_aborted) {
            // It's just normal timer reset. Do nothing.
            return;
        }

        on_error(ec);
    }

    // TODO: Timeout - disconnect all channels.
    throw std::runtime_error("disowned");
}

auto worker_session_t::push(std::uint64_t span, io::encoder_t::message_type&& message) -> future_t<void> {
    auto channels = this->channels.synchronize();
    auto it = channels->find(span);
    if (it == channels->end()) {
        // TODO: Throw a typed exception.
        throw std::runtime_error("trying to send message through non-registered channel");
    }

    return push(std::move(message));
}

auto worker_session_t::push(io::encoder_t::message_type&& message) -> future_t<void> {
    promise_t<void> p;
    auto f = p.get_future();

    loop.post(std::bind(&push_t<worker_session_t>::operator(),
                        std::make_shared<push_t<worker_session_t>>(std::move(message), shared_from_this(), std::move(p))));
    return f;
}

void worker_session_t::on_read(const std::error_code& ec) {
    CF_DBG("read event: %s", CF_EC(ec));

    if (ec) {
        on_error(ec);
        return;
    }

    CF_DBG("event %llu, span %llu", message.type(), message.span());

    switch (message.type()) {
    case (io::event_traits<io::rpc::handshake>::id):
        CF_DBG("-> Handshake");
        // Cocaine runtime should never send handshake event.
        COCAINE_ASSERT(false);
        break;
    case (io::event_traits<io::rpc::heartbeat>::id):
        // We received a heartbeat message from the runtime. Reset the disown timer.
        inhale();
        break;
    case (io::event_traits<io::rpc::terminate>::id):
        //worker->stop();
        break;
    case (io::event_traits<io::rpc::invoke>::id): {
        std::string event;
        io::type_traits<
            typename io::event_traits<io::rpc::invoke>::argument_type
        >::unpack(message.args(), event);
        auto handler = dispatch.get(event);
        if (!handler) {
            // TODO: Log
            // TODO: Notify the runtime about missing event.
            return;
        }

        auto id = message.span();
        auto tx = std::make_shared<basic_sender_t<worker_session_t>>(id, shared_from_this());
        auto ss = std::make_shared<detail::shared_state_t>();
        auto rx = std::make_shared<basic_receiver_t<worker_session_t>>(id, shared_from_this(), ss);

        channels->insert(std::make_pair(id, ss));
        dispatch.post([handler, tx, rx](){ (*handler)(tx, rx); });
        break;
    }
    case (io::event_traits<io::rpc::chunk>::id):
    case (io::event_traits<io::rpc::error>::id):
    case (io::event_traits<io::rpc::choke>::id): {
        auto channels = this->channels.synchronize();
        auto it = channels->find(message.span());
        if (it == channels->end()) {
            // TODO: Log orphan.
            // TODO: It's invariant at this moment.
            return;
        }

        // TODO: Refactor this crap!
        if (message.type() == io::event_traits<io::rpc::chunk>::id) {
            std::string s;
            io::type_traits<
                typename io::event_traits<io::rpc::chunk>::argument_type
            >::unpack(message.args(), s);
            io::encoded<io::streaming<boost::mpl::list<std::string>>::chunk> emsg(message.span(), s);
            io::decoder_t::message_type dmsg;
            std::error_code ec;
            io::decoder_t decoder;
            decoder.decode(emsg.data(), emsg.size(), dmsg, ec);
            it->second->put(std::move(dmsg));
        }
        break;
    }
    default:
        break;
    }

    channel->reader->read(message, std::bind(&worker_session_t::on_read, this, ph::_1));
}

void worker_session_t::on_error(const std::error_code& ec) {
    // TODO: Stop the worker on any network error.
}

void worker_session_t::revoke(std::uint64_t span) {
    // TODO: Make it work.
}
