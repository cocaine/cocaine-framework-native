#include "cocaine/framework/session.hpp"

#include "cocaine/framework/detail/log.hpp"

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
        CF_LOG(detail::logger, detail::debug, "write event: %s", ec.message().c_str());

        if (ec) {
            connection->disconnect(ec);
            h.set_exception(std::system_error(ec));
        } else {
            h.set_value();
        }
    }
};

basic_session_t::basic_session_t(loop_t& loop) noexcept :
    loop(loop),
    state(state_t::disconnected),
    counter(1)
{}

bool basic_session_t::connected() const noexcept {
    return state == state_t::connected;
}

auto basic_session_t::connect(const endpoint_t& endpoint) -> future_t<std::error_code> {
    promise_t<std::error_code> promise;
    auto future = promise.get_future();

    std::lock_guard<std::mutex> lock(mutex);
    switch (state.load()) {
    case state_t::disconnected: {
        std::unique_ptr<socket_type> socket(new socket_type(loop));

        // The code above can throw std::bad_alloc, so here it is the right place to change
        // current object's state.
        // ???
        state = state_t::connecting;

        socket_type* socket_ = socket.get();
        socket_->async_connect(
            endpoint,
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

void basic_session_t::disconnect(const std::error_code& ec) {
    COCAINE_ASSERT(ec); // TODO: Throw invalid_argument.

    auto self = shared_from_this();
    loop.post([self, ec]{
        self->channel.reset();

        // TODO: Consider doing this actions in the asynchronous completion handlers.
        auto channels = self->channels.synchronize();
        for (auto channel : *channels) {
            channel.second->error(ec);
        }
        channels->clear();
    });
}

auto basic_session_t::push(std::uint64_t span, io::encoder_t::message_type&& message) -> future_t<void> {
    auto channels = this->channels.synchronize();
    auto it = channels->find(span);
    if (it == channels->end()) {
        // TODO: Throw a typed exception.
        throw std::runtime_error("trying to send message through non-registered channel");
    }

    // TODO: Traverse in a typed sender.
    return push(std::move(message));
}

auto basic_session_t::push(io::encoder_t::message_type&& message) -> future_t<void> {
    promise_t<void> p;
    auto f = p.get_future();

    loop.post(std::bind(&push_t::operator(),
                        std::make_shared<push_t>(std::move(message), shared_from_this(), std::move(p))));
    return f;
}

void basic_session_t::on_connect(const std::error_code& ec, promise_t<std::error_code>& promise, std::unique_ptr<socket_type>& s) {
    COCAINE_ASSERT(state_t::connecting == state);

    CF_LOG(detail::logger, detail::debug, "connect event: %s", ec.message().c_str());

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
    CF_LOG(detail::logger, detail::debug, "read event: %s", ec.message().c_str());

    if (ec) {
        disconnect(ec);
        return;
    }

    auto channels = this->channels.synchronize();
    auto it = channels->find(message.span());
    if (it == channels->end()) {
        CF_LOG(detail::logger, detail::warn, "dropping an orphan span %llu message", message.span());
    } else {
        it->second->push(std::move(message));
    }

    channel->reader->read(message, std::bind(&basic_session_t::on_read, shared_from_this(), ph::_1));
}
