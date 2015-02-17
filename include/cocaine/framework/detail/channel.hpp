#pragma once

#include <memory>

#include <cocaine/rpc/asio/encoder.hpp>
#include <cocaine/rpc/asio/decoder.hpp>
#include <cocaine/rpc/asio/readable_stream.hpp>
#include <cocaine/rpc/asio/writable_stream.hpp>

namespace cocaine {

namespace framework {

namespace detail {

template<class Protocol, class Encoder = io::encoder_t, class Decoder = io::decoder_t>
struct channel {
    typedef Protocol protocol_type;
    typedef Encoder encoder_type;
    typedef Decoder decoder_type;
    typedef typename protocol_type::socket socket_type;

    explicit
    channel(std::unique_ptr<socket_type> socket):
        socket(std::move(socket)),
        reader(new io::readable_stream<protocol_type, decoder_type>(this->socket)),
        writer(new io::writable_stream<protocol_type, encoder_type>(this->socket))
    {
        this->socket->non_blocking(true);
    }

   ~channel() {
        try {
            socket->shutdown(socket_type::shutdown_both);
            socket->close();
        } catch(const asio::system_error&) {
            // Might be already disconnected by the remote peer, so ignore all errors.
        }
    }

    // The underlying shared socket object.
    const std::shared_ptr<socket_type> socket;

    // Unidirectional channel streams.
    const std::shared_ptr<io::readable_stream<protocol_type, decoder_type>> reader;
    const std::shared_ptr<io::writable_stream<protocol_type, encoder_type>> writer;
};

}

}

}
