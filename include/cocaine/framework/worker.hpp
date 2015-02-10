#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <asio/local/stream_protocol.hpp>

#include <cocaine/common.hpp>
#include <cocaine/idl/node.hpp>
#include <cocaine/rpc/asio/channel.hpp>
#include <cocaine/detail/service/node/messages.hpp>
#include <cocaine/locked_ptr.hpp>

#include "cocaine/framework/sender.hpp"
#include "cocaine/framework/receiver.hpp"

namespace cocaine {

namespace framework {

struct options_t {
    std::string name;
    std::string uuid;
    std::string endpoint;
    std::string locator;

    options_t(int argc, char** argv);
};

template<class Session>
class sender<io::rpc_tag, Session> {
    std::shared_ptr<basic_sender_t<Session>> d;

public:
    sender(std::shared_ptr<basic_sender_t<Session>> d) :
        d(d)
    {}

    template<class... Args>
    future_t<sender>
    write(Args&&... args) {
        auto f = d->template send<io::rpc::chunk>(std::forward<Args>(args)...);

        auto d = this->d;
        return f.then([d](future_t<void>& f){
            f.get();
            return sender<io::rpc_tag, Session>(d);
        });
    }
};

class worker_t;
class worker_session_t;

//! RAII thread pool
class dispatch_t {
    typedef io::stream_of<std::string>::tag streaming_tag;
public:
    typedef sender<io::rpc_tag, worker_session_t> sender_type;
    typedef receiver<streaming_tag, worker_session_t> receiver_type;
    typedef std::function<void(sender_type, receiver_type)> handler_type;

private:
    loop_t loop;
    boost::optional<loop_t::work> work;
    boost::thread_group pool;

    std::unordered_map<std::string, handler_type> handlers;

public:
    dispatch_t() :
        work(loop)
    {
        auto threads = std::thread::hardware_concurrency();
        start(threads != 0 ? threads : 1);
    }

    dispatch_t(unsigned int threads) :
        work(loop)
    {
        if (threads == 0) {
            throw std::invalid_argument("thread count must be a positive number");
        }

        start(threads);
    }

    ~dispatch_t() {
        work.reset();
        pool.join_all();
    }

    boost::optional<handler_type> get(const std::string& event) {
        auto it = handlers.find(event);
        if (it != handlers.end()) {
            return it->second;
        }

        return boost::none;
    }

    template<typename F>
    void on(std::string event, F handler) {
        handlers[event] = std::move(handler);
    }

    void post(std::function<void()> fn) {
        loop.post(fn);
    }

private:
    void start(unsigned int threads) {
        for (unsigned int i = 0; i < threads; ++i) {
            pool.create_thread(
                std::bind(static_cast<std::size_t(loop_t::*)()>(&loop_t::run), std::ref(loop))
            );
        }
    }
};

class worker_session_t : public std::enable_shared_from_this<worker_session_t> {
public:
    typedef io::stream_of<std::string>::tag streaming_tag;

    typedef sender<io::rpc_tag, worker_session_t> sender_type;
    typedef receiver<streaming_tag, worker_session_t> receiver_type;

    typedef asio::local::stream_protocol protocol_type;
    typedef protocol_type::socket socket_type;
    typedef io::channel<protocol_type> channel_type;

    typedef std::function<void(sender_type, receiver_type)> handler_type;

    template<class>
    class push_t;

private:
    loop_t& loop;
    dispatch_t& dispatch;

    io::decoder_t::message_type message;
    std::unique_ptr<channel_type> channel;
    synchronized<std::unordered_map<std::uint64_t, std::shared_ptr<detail::shared_state_t>>> channels;

    // Health.
    asio::deadline_timer heartbeat_timer;
    asio::deadline_timer disown_timer;

public:
    worker_session_t(loop_t& loop, dispatch_t& dispatch);

    void connect(std::string endpoint, std::string uuid);

    auto push(std::uint64_t span, io::encoder_t::message_type&& message) -> future_t<void>;
    auto push(io::encoder_t::message_type&& message) -> future_t<void>;
    void revoke(std::uint64_t span);

    void terminate(const std::error_code& ec);

private:
    void on_read(const std::error_code& ec);
    void on_error(const std::error_code& ec);

    void handshake(const std::string& uuid);

    // Called after receiving a heartbeat event from the runtime. Reset disown timer.
    void inhale();

    // Called on timer, send heartbeat to the runtime, restart the heartbeat timer.
    void exhale(const std::error_code& ec = std::error_code());
};

class worker_t {
public:
    loop_t loop;

    options_t options;
    dispatch_t dispatch;
    std::shared_ptr<worker_session_t> session;

public:
    worker_t(options_t options);

    template<class F>
    void on(std::string event, F handler) {
        dispatch.on(event, std::move(handler));
    }

    int run();
};

} // namespace framework

} // namespace cocaine
