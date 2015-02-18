#include "cocaine/framework/session.hpp"

#include <asio/connect.hpp>

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/detail/loop.hpp"
#include "cocaine/framework/scheduler.hpp"
#include "cocaine/framework/util/net.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;

//! \note single shot.
class basic_session_t::push_t : public std::enable_shared_from_this<push_t> {
    io::encoder_t::message_type message;
    std::shared_ptr<basic_session_t> connection;
    promise_t<void> h;

public:
    explicit push_t(io::encoder_t::message_type&& message, std::shared_ptr<basic_session_t> connection, promise_t<void>&& h) :
        message(std::move(message)),
        connection(connection),
        h(std::move(h))
    {}

    void operator()() {
        if (connection->channel) {
            connection->channel->writer->write(message, std::bind(&push_t::on_write, shared_from_this(), ph::_1));
        } else {
            h.set_exception(std::system_error(io_provider::error::not_connected));
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

basic_session_t::basic_session_t(scheduler_t& scheduler) noexcept :
    scheduler(scheduler),
    state(state_t::disconnected),
    counter(1),
    message(boost::none)
{}

basic_session_t::~basic_session_t() {
    CF_DBG("destroying basic session (%lu active) ...", channels->size());
}

bool basic_session_t::connected() const noexcept {
    return state == state_t::connected;
}

auto basic_session_t::connect(const endpoint_type& endpoint) -> future_type<std::error_code> {
    return connect(std::vector<endpoint_type> {{ endpoint }});
}

auto basic_session_t::connect(const std::vector<endpoint_type>& endpoints) -> future_type<std::error_code> {
    promise_type<std::error_code> promise;
    auto future = promise.get_future();

    std::lock_guard<std::mutex> lock(mutex);
    switch (state.load()) {
    case state_t::disconnected: {
        std::unique_ptr<socket_type> socket(new socket_type(scheduler.loop().loop));

        // The code above can throw std::bad_alloc, so here it is the right place to change
        // current object's state.
        state = state_t::connecting;

        auto converted = util::endpoints_cast<asio::ip::tcp::endpoint>(endpoints);
        socket_type* socket_ = socket.get();
        asio::async_connect(
            *socket_,
            converted.begin(), converted.end(),
            std::bind(&basic_session_t::on_connect, shared_from_this(), ph::_1, std::move(promise), std::move(socket))
        );

        break;
    }
    case state_t::connecting: {
        promise.set_value(io_provider::error::already_started);
        break;
    }
    case state_t::connected: {
        promise.set_value(io_provider::error::already_connected);
        break;
    }
    default:
        COCAINE_ASSERT(false);
    }

    return future;
}

void basic_session_t::disconnect() {
    CF_DBG("disconnecting basic session ...");
    auto this_ = shared_from_this();
    scheduler([this_]{
        this_->channel.reset();
        CF_DBG("disconnecting basic session - done");
    });
}

auto basic_session_t::push(std::uint64_t span, io::encoder_t::message_type&& message) -> future_t<void> {
    // TODO: There are mute events, which only push, but doesn't listen.
    auto channels = this->channels.synchronize();
    auto it = channels->find(span);
    if (it == channels->end()) {
        // TODO: Throw a typed exception.
        throw std::runtime_error("trying to send message through non-registered channel");
    }

    return push(std::move(message));
}

auto basic_session_t::push(io::encoder_t::message_type&& message) -> future_t<void> {
    promise_t<void> p;
    auto f = p.get_future();

    scheduler(std::bind(&push_t::operator(),
                        std::make_shared<push_t>(std::move(message), shared_from_this(), std::move(p))));
    return f;
}

void basic_session_t::revoke(std::uint64_t span) {
    CF_DBG("revoking span %llu channel", span);

    auto this_ = shared_from_this();
    scheduler([this_, span]{
        this_->channels->erase(span);
    });
}

auto basic_session_t::next() -> std::uint64_t {
    return counter++;
}

auto
basic_session_t::invoke(std::uint64_t span, io::encoder_t::message_type&& message) -> future_t<basic_session_t::basic_invocation_result> {
    auto tx = std::make_shared<basic_sender_t<basic_session_t>>(span, shared_from_this());
    auto state = std::make_shared<detail::shared_state_t>();
    auto rx = std::make_shared<basic_receiver_t<basic_session_t>>(span, shared_from_this(), state);

    // TODO: Do not insert mute channels.
    channels->insert(std::make_pair(span, state));
    auto f1 = push(std::move(message));
    auto f2 = f1.then([tx, rx](future_t<void>& f){ // TODO: Executor!
        f.get();
        return std::make_tuple(tx, rx);
    });

    return f2;
}

void basic_session_t::on_connect(const std::error_code& ec, promise_t<std::error_code>& promise, std::unique_ptr<socket_type>& s) {
    CF_DBG("connect event: %s", CF_EC(ec));
    COCAINE_ASSERT(state_t::connecting == state);

    if (ec) {
        channel.reset();
        state = state_t::disconnected;
    } else {
        channel.reset(new channel_type(std::move(s)));
        channel->reader->read(message, std::bind(&basic_session_t::on_read, shared_from_this(), ph::_1));
        state = state_t::connected;
    }

    promise.set_value(ec);
}

void basic_session_t::on_read(const std::error_code& ec) {
    CF_DBG("read event: %s", CF_EC(ec));

    if (ec) {
        on_error(ec);
        return;
    }

    if (!channel) {
        on_error(io_provider::error::operation_aborted);
        return;
    }

    CF_DBG("received message [%llu, %llu]", message.span(), message.type());
    auto channels = this->channels.synchronize();
    auto it = channels->find(message.span());
    if (it == channels->end()) {
        CF_DBG("dropping an orphan span %llu message", message.span());
    } else {
        it->second->put(std::move(message));
    }

    channel->reader->read(message, std::bind(&basic_session_t::on_read, shared_from_this(), ph::_1));
}

void basic_session_t::on_error(const std::error_code& ec) {
    COCAINE_ASSERT(ec);

    state = state_t::disconnected;

    auto channels = this->channels.synchronize();
    for (auto channel : *channels) {
        channel.second->put(ec);
    }
    channels->clear();
}
