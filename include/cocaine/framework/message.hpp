#pragma once

#include <cstdint>
#include <memory>

#include <boost/none_t.hpp>

namespace msgpack { struct object; }

namespace cocaine {

namespace framework {

class decoded_message {
    class impl;
    std::unique_ptr<impl> d;

public:
    explicit decoded_message(boost::none_t);
    decoded_message(const char* data, size_t size);
    ~decoded_message();

    decoded_message(decoded_message&& other);
    decoded_message& operator=(decoded_message&& other);

    auto span() const -> uint64_t;
    auto type() const -> uint64_t;
    auto args() const -> const msgpack::object&;
};

} // namespace framework

} // namespace cocaine
