/*
    Copyright (c) 2015 Evgeny Safronov <division494@gmail.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.
    This file is part of Cocaine.
    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "cocaine/framework/worker/error.hpp"
#include "cocaine/framework/worker/http/request.hpp"
#include "cocaine/framework/worker/http/response.hpp"
#include "cocaine/framework/worker/sender.hpp"
#include "cocaine/framework/worker/receiver.hpp"

namespace cocaine {

namespace framework {

namespace worker {

/*!
 * The basic middleware trait.
 *
 * This middleware defines request/response types and pair of mapping functions working with them.
 *
 * This trait provides basic functionality for handling HTTP events.
 *
 * If you want to work with your own HTTP request/response classes, you should define yown own
 * middleware and provide mapping functions and typedefs in it.
 */
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
    /// The status indicating headers have not been read (for receiver) or written (for sender).
    class fresh;

    /// The status indicating headers have been read (for receiver) or written (for sender).
    class streaming;

    template<class Middleware = basic_middleware>
    struct event {
        typedef http_sender<fresh, Middleware> fresh_sender;
        typedef http_sender<streaming, Middleware> streaming_sender;

        typedef http_receiver<fresh, Middleware> fresh_receiver;
        typedef http_receiver<streaming, Middleware> streaming_receiver;
    };
};

/*!
 * The HTTP streaming sender provides API for sequential writing body chunks to the stream.
 *
 * \note the stream will be automatically closed when an object of this class has run out of the
 * scope.
 */
template<class M>
class http_sender<http::streaming, M> {
    sender tx;

public:
    /*!
     * \note this constructor is intentionally left implicit.
     */
    http_sender(sender tx) :
        tx(std::move(tx))
    {}

    http_sender(const http_sender&) = delete;
    http_sender(http_sender&& other) = default;

    http_sender& operator=(const http_sender&) = delete;
    http_sender& operator=(http_sender&& other) = default;

    /*!
     * Sends another portion of data (mostly body) to the stream.
     *
     * \return a future, which will be set with the following HTTP sender when the data is
     * completely sent to the underlying buffer of an OS or any network error occured.
     *
     * \warning the current object will be invalidated after this call.
     */
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
    unwrap(task<sender>::future_move_type future) {
        return future.get();
    }
};

/*!
 * The HTTP fresh sender provides API for writing the first response information, like status code
 * and headers.
 *
 * It provides a streaming HTTP sender object after first send invocation and is considered
 * descructed after this.
 */
template<class M>
class http_sender<http::fresh, M> {
    sender tx;

public:
    /*!
     * \note this constructor is intentionally left implicit.
     */
    http_sender(sender tx) :
        tx(std::move(tx))
    {}

    http_sender(const http_sender&) = delete;
    http_sender(http_sender&& other) = default;

    http_sender& operator=(const http_sender&) = delete;
    http_sender& operator=(http_sender&& other) = default;

    /*!
     * Encodes the response object with the underlying protocol and writes generated message to the
     * stream.
     *
     * \warning the current object will be invalidated after this call.
     */
    typename task<http_sender<http::streaming, M>>::future_type
    send(typename M::response_type response) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> packer(buffer);
        io::type_traits<
            http_response_t
        >::pack(packer, M::map_response(std::move(response)));

        auto tx = std::move(this->tx);
        auto future = tx.write(std::string(buffer.data(), buffer.size()));
        return future
            .then(std::bind(&http_sender::transform, std::placeholders::_1));
    }

private:
    static
    http_sender<http::streaming, M>
    transform(task<sender>::future_move_type future) {
        auto tx = future.get();
        return http_sender<http::streaming, M>(std::move(tx));
    }
};

/*!
 * The streaming HTTP receiver provides a streaming-like API to receive body chunks from the stream.
 *
 * Unlike other receivers it can be both copyed and moved.
 */
template<class M>
class http_receiver<http::streaming, M> {
    receiver rx;

    // I do not use boost::optional here, because of its late move-semantics support.
    bool cached;
    std::string body;

public:
    /*!
     * Constructs a streaming HTTP receiver using the underlying string receiver.
     *
     * The body field is left uninitialized.
     *
     * \note this constructor is intentionally left implicit.
     */
    http_receiver(receiver rx) :
        rx(std::move(rx)),
        cached(false)
    {}

    /*!
     * Constructs a streaming HTTP receiver using the underlying string receiver and the body
     * string.
     */
    http_receiver(receiver rx, std::string body) :
        rx(std::move(rx)),
        cached(true),
        body(std::move(body))
    {}

    http_receiver(const http_receiver& other) = default;
    http_receiver(http_receiver&& other) = default;

    http_receiver& operator=(const http_receiver& other) = default;
    http_receiver& operator=(http_receiver&& other) = default;

    /*!
     * Tries to receive another chunk of data from the stream.
     *
     * \return a future, which will be set after receiving the next message from the stream. It may
     * contain none value, indicating that the other side has closed the stream for writing.
     */
    task<boost::optional<std::string>>::future_type
    recv() {
        if (cached) {
            cached = false;
            return make_ready_future<boost::optional<std::string>>::value(body);
        }

        return rx.recv();
    }
};

/*!
 * The fresh HTTP receiver provides an ability to receive the first HTTP request information, like
 * method, uri or headers.
 */
template<class M>
class http_receiver<http::fresh, M> {
    typedef M middleware_type;
    typedef typename middleware_type::request_type request_type;

    receiver rx;

public:
    /*!
     * Constructs a fresh HTTP receiver using the underlying string receiver.
     *
     * \note this constructor is intentionally left implicit.
     */
    http_receiver(receiver rx) :
        rx(std::move(rx))
    {}

    http_receiver(const http_receiver&) = delete;
    http_receiver(http_receiver&& other) = default;

    http_receiver& operator=(const http_receiver&) = delete;
    http_receiver& operator=(http_receiver&& other) = default;

    /*!
     * Tries to receive the first HTTP request information object.
     *
     * If succeed it also returns a streaming HTTP receiver, which can be used to fetch body chunks.
     *
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
    transform(task<boost::optional<std::string>>::future_move_type future, receiver rx) {
        auto unpacked = future.get();

        if (!unpacked) {
            throw unexpected_eof();
        }

        msgpack::unpacked msg;
        msgpack::unpack(&msg, unpacked->data(), unpacked->size());

        http_request_t request;
        std::string body;
        io::type_traits<http_request_t>::unpack(msg.get(), request, body);

        return std::make_tuple(
            M::map_request(std::move(request)),
            http_receiver<http::streaming, M>(std::move(rx), std::move(body))
        );
    }
};

/*!
 * The transform traits specialization for HTTP events.
 */
template<class Dispatch, class Middleware>
struct transform_traits<Dispatch, http::event<Middleware>> {
    typedef http_sender<http::fresh, Middleware> sender_type;
    typedef http_receiver<http::fresh, Middleware> receiver_type;
    typedef std::function<void(sender_type, receiver_type)> input_type;

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
