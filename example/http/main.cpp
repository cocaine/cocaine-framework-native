#include <iostream>

#include <boost/optional.hpp>

#include <cocaine/framework/worker.hpp>

// #include <cocaine/framework/worker/http.inl>
#include <cocaine/traits/tuple.hpp>
#include <swarm/http_request.hpp>
#include <swarm/http_response.hpp>

namespace cocaine {

namespace framework {

namespace worker {

struct http {
    class fresh;
    class streaming;
};

template<class T>
class http_sender;

template<>
class http_sender<http::streaming> {
    sender tx;

public:
    http_sender(sender tx) :
        tx(std::move(tx))
    {}
};

template<>
class http_sender<http::fresh> {
    sender tx;

public:
    http_sender(sender tx) :
        tx(std::move(tx))
    {}

    // TODO: Movable, noncopyable.

    typename task<http_sender<http::streaming>>::future_type
    send(ioremap::swarm::http_response rs) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> packer(&buffer);
        io::type_traits<
            boost::mpl::list<
                int,
                std::vector<std::pair<std::string, std::string>>
            >
        >::pack(packer, rs.code(), rs.headers().all());

        auto tx = std::move(this->tx);
        auto future = tx.write(std::string(buffer.data(), buffer.size()));
        return future.then(std::bind(&http_sender::transform, std::placeholders::_1));
    }

private:
    static
    http_sender<http::streaming>
    transform(typename task<sender>::future_move_type future) {
        auto tx = future.get();
        return http_sender<http::streaming>(std::move(tx));
    }
};

template<class T>
class http_receiver;

template<>
class http_receiver<http::streaming> {
    receiver rx;
    bool cached;
    std::string body;

public:
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

template<>
class http_receiver<http::fresh> {
    receiver rx;

public:
    http_receiver(receiver rx) :
        rx(std::move(rx))
    {}

    // TODO: Movable, noncopyable.

    /*!
     * \warning the object will be invalidated after this call.
     */
    typename task<
        std::tuple<
            ioremap::swarm::http_request,
            http_receiver<http::streaming>
        >
    >::future_type
    recv() {
        auto rx = std::move(this->rx);
        return rx.recv()
            .then(std::bind(&http_receiver::transform, std::placeholders::_1, rx));
    }

private:
    static
    std::tuple<
        ioremap::swarm::http_request,
        http_receiver<http::streaming>
    >
    transform(typename task<boost::optional<std::string>>::future_move_type future, receiver rx) {
        auto unpacked = future.get();

        if (!unpacked) {
            // TODO: Throw more typed exceptions.
            throw std::runtime_error("received unexpected choke");
        }

        msgpack::unpacked msg;
        msgpack::unpack(&msg, unpacked->data(), unpacked->size());

        std::string method;
        std::string uri;
        std::string version;
        std::vector<std::pair<std::string, std::string>> headers;
        std::string body;

        io::type_traits<
            boost::mpl::list<
                std::string,
                std::string,
                std::string,
                std::vector<std::pair<std::string, std::string>>,
                std::string
            >
        >::unpack(msg.get(), method, uri, version, headers, body);

        ioremap::swarm::http_request rq;
        rq.set_method(method);
        rq.set_url(uri);
        rq.headers().assign(std::move(headers));

        return std::make_tuple(std::move(rq), http_receiver<http::streaming>(std::move(rx), std::move(body)));
    }
};

template<class Dispatch>
struct transform_traits<Dispatch, http> {
    typedef std::function<void(http_sender<http::fresh>, http_receiver<http::fresh>)> input_type;

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

int main(int argc, char** argv) {
    worker_t worker(options_t(argc, argv));

    worker.on<http>("http", [](http_sender<http::fresh> tx, http_receiver<http::fresh>){
        swarm::http_response rs;
        rs.set_code(200);
        tx.send(rs).get();
    });

    return worker.run();
}
