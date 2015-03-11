#include <iostream>

#include <boost/optional.hpp>

#include <cocaine/framework/worker.hpp>

// #include <cocaine/framework/worker/http.inl>

#include <cocaine/traits/tuple.hpp>
#include <cocaine/traits/vector.hpp>

#include <swarm/http_request.hpp>
#include <swarm/http_response.hpp>

/// #include <cocaine/framework/worker/http/request.hpp>
#include <string>
#include <vector>

namespace cocaine {

namespace framework {

namespace worker {

struct http_request_t {
    std::string method;
    std::string uri;
    std::string version;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

} // namespace worker

} // namespace framework

namespace io {

template<>
struct type_traits<framework::worker::http_request_t> {
    typedef boost::mpl::list<
        std::string,
        std::string,
        std::string,
        std::vector<std::pair<std::string, std::string>>,
        std::string
    > tuple_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const framework::worker::http_request_t& source) {
        io::type_traits<
            tuple_type
        >::pack(packer, source.method, source.uri, source.version, source.headers, source.body);
    }

    static inline
    void
    unpack(const msgpack::object& unpacked, framework::worker::http_request_t& target) {
        io::type_traits<
            tuple_type
        >::unpack(unpacked, target.method, target.uri, target.version, target.headers, target.body);
    }
};

} // namespace io

} // namespace cocaine

////////////////////////////////////////////////////////////////////////////////////////////////////

/// #include <cocaine/framework/worker/http/response.hpp>
#include <string>
#include <vector>

namespace cocaine {

namespace framework {

namespace worker {

struct http_response_t {
    int code;
    std::vector<std::pair<std::string, std::string>> headers;
};

} // namespace worker

} // namespace framework

namespace io {

template<>
struct type_traits<framework::worker::http_response_t> {
    typedef boost::mpl::list<
        int,
        std::vector<std::pair<std::string, std::string>>
    > tuple_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const framework::worker::http_response_t& source) {
        io::type_traits<
            tuple_type
        >::pack(packer, source.code, source.headers);
    }

    static inline
    void
    unpack(const msgpack::object& unpacked, framework::worker::http_response_t& target) {
        io::type_traits<
            tuple_type
        >::unpack(unpacked, target.code, target.headers);
    }
};

} // namespace io

} // namespace cocaine

////////////////////////////////////////////////////////////////////////////////////////////////////

/// #include <cocaine/framework/worker/http/swarm.inl>

namespace cocaine {

} // namespace cocaine

////////////////////////////////////////////////////////////////////////////////////////////////////

/// #include <cocaine/framework/worker/http.hpp>

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

}

} // namespace framework

} // namespace cocaine

using namespace cocaine::framework;
using namespace cocaine::framework::worker;

using namespace ioremap;

struct swarm_middleware {
    typedef swarm::http_request request_type;
    typedef swarm::http_response response_type;

    static
    request_type
    map_request(http_request_t&& rq) {
        request_type result;
        result.set_method(rq.method);
        result.set_url(rq.uri);
        result.headers().assign(std::move(rq.headers));
        return result;
    }

    static
    http_response_t
    map_response(response_type&& rs) {
        http_response_t result;
        result.code = rs.code();
        result.headers = std::move(rs.headers().all());
        return result;
    }
};

int main(int argc, char** argv) {
    worker_t worker(options_t(argc, argv));

    typedef http::event<> http_t;
    worker.on<http_t>("http", [](http_t::fresh_sender tx, http_t::fresh_receiver){
        http_response_t rs;
        rs.code = 200;
        tx.send(std::move(rs)).get()
            .send("Hello from C++").get();
    });

    typedef http::event<swarm_middleware> http_swarm;
    worker.on<http_swarm>("http_swarm", [](http_swarm::fresh_sender tx, http_swarm::fresh_receiver){
        swarm::http_response rs;
        rs.set_code(200);
        tx.send(std::move(rs)).get()
            .send("Hello from C++ and Swarm").get();
    });

    return worker.run();
}
