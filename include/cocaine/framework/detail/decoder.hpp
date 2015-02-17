#pragma once

#include <memory>
#include <system_error>

#include <boost/none_t.hpp>

// TODO: Detail forwards.
namespace msgpack { struct object; }

namespace cocaine {

namespace framework {

namespace detail {

class decoded_message {
    class impl;
    std::unique_ptr<impl> d;

public:
    decoded_message(boost::none_t);
    decoded_message(const char* data, size_t size);
    ~decoded_message();

    decoded_message(decoded_message&& other);
    decoded_message& operator=(decoded_message&& other);

    auto span() const -> uint64_t;
    auto type() const -> uint64_t;
    auto args() const -> const msgpack::object&;
};

struct decoder_t {
    typedef decoded_message message_type;

    size_t decode(const char* data, size_t size, message_type& message, std::error_code& ec);
};

}

}

}
