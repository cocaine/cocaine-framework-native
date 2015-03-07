#include "cocaine/framework/detail/basic_session.hpp"

#include <memory>

#include <asio/connect.hpp>

#include "cocaine/framework/sender.hpp"
#include "cocaine/framework/scheduler.hpp"

#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/detail/loop.hpp"
#include "cocaine/framework/detail/net.hpp"
#include "cocaine/framework/detail/shared_state.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;
using namespace cocaine::framework::detail;

//! \note single shot.
class basic_session_t::push_t : public std::enable_shared_from_this<push_t> {
    io::encoder_t::message_type message;
    std::shared_ptr<basic_session_t> connection;
    typename task<void>::promise_type promise;

public:
    push_t(io::encoder_t::message_type&& message, std::shared_ptr<basic_session_t> connection, typename task<void>::promise_type&& promise) :
        message(std::move(message)),
        connection(connection),
        promise(std::move(promise))
    {}

    /*!
     * \warning guaranteed to be called from the event loop thread, otherwise the behavior is
     * undefined.
     */
    void operator()() {
        if (connection->channel) {
            CF_DBG("writing %lu bytes ...", message.size());
            connection->channel->writer->write(message, wrap(std::bind(&push_t::on_write, shared_from_this(), ph::_1)));
        } else {
            CF_DBG("<< write aborted: not connected");
            promise.set_exception(std::system_error(asio::error::not_connected));
        }
    }

private:
    void on_write(const std::error_code& ec) {
        CF_DBG("<< write: %s", CF_EC(ec));

        if (ec) {
            connection->on_error(ec);
            promise.set_exception(std::system_error(ec));
        } else {
            promise.set_value();
        }
    }
};

basic_session_t::basic_session_t(scheduler_t& scheduler) noexcept :
    scheduler(scheduler),
    state(0),
    counter(1),
    message(boost::none)
{}

basic_session_t::~basic_session_t() {}

bool basic_session_t::connected() const noexcept {
    return state == static_cast<std::uint8_t>(state_t::connected);
}

auto basic_session_t::connect(const endpoint_type& endpoint) -> typename task<std::error_code>::future_type {
    return connect(std::vector<endpoint_type> {{ endpoint }});
}

auto basic_session_t::connect(const std::vector<endpoint_type>& endpoints) -> typename task<std::error_code>::future_type {
    CF_CTX("bC");
    CF_DBG(">> connecting ...");

    typename task<std::error_code>::promise_type promise;
    auto future = promise.get_future();

    std::lock_guard<std::mutex> lock(mutex);
    switch (static_cast<state_t>(state.load())) {
    case state_t::disconnected: {
        std::unique_ptr<socket_type> socket(new socket_type(scheduler.loop().loop));

        // The code above can throw std::bad_alloc, so here it is the right place to change
        // current object's state.
        state = static_cast<std::uint8_t>(state_t::connecting);

        auto converted = endpoints_cast<asio::ip::tcp::endpoint>(endpoints);
        socket_type* socket_ = socket.get();
        asio::async_connect(
            *socket_,
            converted.begin(), converted.end(),
            wrap(std::bind(&basic_session_t::on_connect, shared_from_this(), ph::_1, std::move(promise), std::move(socket)))
        );

        break;
    }
    case state_t::connecting: {
        CF_DBG("<< already in progress");

        promise.set_value(asio::error::already_started);
        break;
    }
    case state_t::connected: {
        CF_DBG("<< already connected");

        promise.set_value(asio::error::already_connected);
        break;
    }
    default:
        COCAINE_ASSERT(false);
    }

    return future;
}

auto basic_session_t::endpoint() const -> boost::optional<endpoint_type> {
    // TODO: Implement me.
    return boost::none;
}

void basic_session_t::disconnect() {
    CF_DBG(">> disconnecting ...");
    scheduler(wrap(std::bind(&basic_session_t::on_disconnect, shared_from_this())));
}

auto basic_session_t::invoke(std::function<io::encoder_t::message_type(std::uint64_t)> encoder)
    -> typename task<invoke_result>::future_type
{
    packaged_task<async<basic_session_t::invoke_result>::future()> task(
        std::bind(&basic_session_t::invoke_deferred, shared_from_this(), std::move(encoder))
    );

    scheduler(task);
    return task.get_future();
//    const std::uint64_t span = counter++;
//    return invoke(span, encoder(span));
}

async<basic_session_t::invoke_result>::future
basic_session_t::invoke_deferred(std::function<io::encoder_t::message_type(std::uint64_t)> encoder) {
    // Guaranteed to be invoked from the event loop thread.
    // TODO: COCAINE_ASSERT(std::this_thread::get_id() == scheduler.thread_id());
    const auto span = counter++;
    return invoke(span, encoder(span));
}

auto
basic_session_t::invoke(std::uint64_t span, io::encoder_t::message_type&& message) -> typename task<invoke_result>::future_type {
    CF_CTX("bI" + std::to_string(span));
    CF_DBG("invoking span %llu event ...", CF_US(span));

    auto tx = std::make_shared<basic_sender_t<basic_session_t>>(span, shared_from_this());
    auto state = std::make_shared<shared_state_t>();
    auto rx = std::make_shared<basic_receiver_t<basic_session_t>>(span, shared_from_this(), state);

    channels->insert(std::make_pair(span, state));
    return push(std::move(message))
        .then(scheduler, wrap([tx, rx](typename task<void>::future_move_type future){
            future.get();
            return std::make_tuple(tx, rx);
        }));
}

auto basic_session_t::push(io::encoder_t::message_type&& message) -> typename task<void>::future_type {
    CF_CTX("bP");
    CF_DBG(">> writing message ...");

    typename task<void>::promise_type promise;
    auto future = promise.get_future();

    scheduler(wrap(std::bind(&push_t::operator(),
                   std::make_shared<push_t>(std::move(message), shared_from_this(), std::move(promise)))));
    return future;
}

void basic_session_t::revoke(std::uint64_t span) {
    CF_DBG(">> revoking span %llu channel", CF_US(span));

    scheduler(wrap(std::bind(&basic_session_t::on_revoke, shared_from_this(), span)));
}

void basic_session_t::on_disconnect() {
    CF_DBG("<< disconnected");

    state = static_cast<std::uint8_t>(state_t::dying);
    if (channels->empty()) {
        CF_DBG("<< stop listening");
        channel.reset();
    }
}

void basic_session_t::on_revoke(std::uint64_t span) {
    CF_DBG("<< revoke span %llu channel", CF_US(span));

    auto channels = this->channels.synchronize();
    channels->erase(span);
    if (channels->empty() && state == static_cast<std::uint8_t>(state_t::dying)) {
        // At this moment there are no references left to this session and also nobody is intrested
        // for data reading.
        // TODO: But there can be pending writing events.
        CF_DBG("<< stop listening");
        channel.reset();
    }
}

void basic_session_t::on_connect(const std::error_code& ec, typename task<std::error_code>::promise_type& promise, std::unique_ptr<socket_type>& s) {
    CF_DBG("<< connect: %s", CF_EC(ec));

    COCAINE_ASSERT(static_cast<std::uint8_t>(state_t::connecting) == state); // TODO: Not always true?

    if (ec) {
        channel.reset();
        state = static_cast<std::uint8_t>(state_t::disconnected);
    } else {
        CF_CTX_POP();
        CF_CTX("bR");
        CF_DBG(">> listening for read events ...");

        channel.reset(new channel_type(std::move(s)));
        channel->reader->read(message, wrap(std::bind(&basic_session_t::on_read, shared_from_this(), ph::_1)));
        state = static_cast<std::uint8_t>(state_t::connected);
    }

    promise.set_value(ec);
}

void basic_session_t::on_read(const std::error_code& ec) {
    CF_DBG("<< read: %s", CF_EC(ec));

    if (ec) {
        on_error(ec);
        return;
    }

    if (!channel) {
        CF_DBG("received message from disconnected channel");
        on_error(asio::error::operation_aborted);
        return;
    }

    CF_DBG("received message [%llu, %llu, %s]", CF_US(message.span()), CF_US(message.type()), CF_MSG(message.args()).c_str());
    auto channels = this->channels.synchronize();
    auto it = channels->find(message.span());
    if (it == channels->end()) {
        CF_DBG("dropping an orphan span %llu message", CF_US(message.span()));
    } else {
        // TODO: Probably it will be better to conditionally stop listening if no channels left.
        // Then I need a bool flag here indicating that there won't be messages more.
        it->second->put(std::move(message));
    }

    CF_DBG(">> listening for read events ...");
    channel->reader->read(message, wrap(std::bind(&basic_session_t::on_read, shared_from_this(), ph::_1)));
}

void basic_session_t::on_error(const std::error_code& ec) {
    COCAINE_ASSERT(ec);

    state = static_cast<std::uint8_t>(state_t::disconnected);

    auto channels = this->channels.synchronize();
    for (auto channel : *channels) {
        channel.second->put(ec);
    }
    channels->clear();
}

#include "sender.cpp"
template class cocaine::framework::basic_sender_t<basic_session_t>;

#include "receiver.cpp"
template class cocaine::framework::basic_receiver_t<basic_session_t>;
