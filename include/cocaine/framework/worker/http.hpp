#pragma once

#include "cocaine/framework/worker/http/request.hpp"
#include "cocaine/framework/worker/http/response.hpp"

namespace cocaine {

namespace framework {

namespace worker {

struct basic_middleware {
    typedef http_request_t request_type;
    typedef http_response_t response_type;

    static
    request_type
    map_request(http_request_t&& rq) {
        return rq;
    }

    static
    http_response_t
    map_response(response_type&& rs) {
        return rs;
    }
};

template<class State, class Middleware>
class http_sender;

template<class State, class Middleware>
class http_receiver;

struct http {
    class fresh;
    class streaming;

    template<class Middleware = basic_middleware>
    struct event {
        typedef http_sender<fresh, Middleware> fresh_sender;
        typedef http_sender<streaming, Middleware> streaming_sender;

        typedef http_receiver<fresh, Middleware> fresh_receiver;
        typedef http_receiver<streaming, Middleware> streaming_receiver;
    };
};

template<class M>
class http_sender<http::streaming, M> {
    sender tx;

public:
    http_sender(sender tx) :
        tx(std::move(tx))
    {}

    // Movable, noncopyable.

    typename task<http_sender>::future_type
    send(std::string data) {
        auto tx = std::move(this->tx);
        auto future = tx.write(std::move(data));
        return future
            .then(std::bind(&http_sender::unwrap, std::placeholders::_1));
    }

private:
    static
    http_sender
    unwrap(typename task<sender>::future_move_type future) {
        return future.get();
    }
};

template<class M>
class http_sender<http::fresh, M> {
    sender tx;

public:
    /// Implicit.
    http_sender(sender tx) :
        tx(std::move(tx))
    {}

    // TODO: Movable, noncopyable.

    typename task<http_sender<http::streaming, M>>::future_type
    send(typename M::response_type rs) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> packer(buffer);
        io::type_traits<
            http_response_t
        >::pack(packer, M::map_response(std::move(rs)));

        auto tx = std::move(this->tx);
        auto future = tx.write(std::string(buffer.data(), buffer.size()));
        return future
            .then(std::bind(&http_sender::transform, std::placeholders::_1));
    }

private:
    static
    http_sender<http::streaming, M>
    transform(typename task<sender>::future_move_type future) {
        auto tx = future.get();
        return http_sender<http::streaming, M>(std::move(tx));
    }
};

template<class M>
class http_receiver<http::streaming, M> {
    receiver rx;
    bool cached;
    std::string body;

public:
    /// Implicit
    http_receiver(receiver rx) :
        rx(std::move(rx)),
        cached(false)
    {}

    http_receiver(receiver rx, std::string body) :
        rx(std::move(rx)),
        cached(true),
        body(std::move(body))
    {}

    // TODO: Movable, copyable.

    typename task<boost::optional<std::string>>::future_type
    recv() {
        if (cached) {
            cached = false;
            return make_ready_future<boost::optional<std::string>>::value(body);
        }

        return rx.recv();
    }
};

template<class M>
class http_receiver<http::fresh, M> {
    typedef M middleware_type;
    typedef typename middleware_type::request_type request_type;

    receiver rx;

public:
    /// Implicit
    http_receiver(receiver rx) :
        rx(std::move(rx))
    {}

    // TODO: Movable, noncopyable.

    /*!
     * \warning the object will be invalidated after this call.
     */
    typename task<std::tuple<request_type, http_receiver<http::streaming, M>>>::future_type
    recv() {
        auto rx = std::move(this->rx);
        return rx.recv()
            .then(std::bind(&http_receiver::transform, std::placeholders::_1, rx));
    }

private:
    static
    std::tuple<typename M::request_type, http_receiver<http::streaming, M>>
    transform(typename task<boost::optional<std::string>>::future_move_type future, receiver rx) {
        auto unpacked = future.get();

        if (!unpacked) {
            // TODO: Throw more typed exceptions.
            throw std::runtime_error("the runtime has unexpectedly closed the channel");
        }

        msgpack::unpacked msg;
        msgpack::unpack(&msg, unpacked->data(), unpacked->size());

        http_request_t rq;
        io::type_traits<http_request_t>::unpack(msg.get(), rq);

        return std::make_tuple(M::map_request(std::move(rq)), http_receiver<http::streaming, M>(std::move(rx), std::move(rq.body)));
    }
};

template<class Dispatch, class Middleware>
struct transform_traits<Dispatch, http::event<Middleware>> {
    typedef std::function<void(http_sender<http::fresh, Middleware>, http_receiver<http::fresh, Middleware>)> input_type;

    static
    typename Dispatch::handler_type
    apply(input_type handler) {
        return [handler](sender tx, receiver rx) {
            handler(std::move(tx), std::move(rx));
        };
    }
};

} // namespace worker

} // namespace framework

} // namespace cocaine
