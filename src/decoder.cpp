#include "cocaine/framework/detail/decoder.hpp"

#include <vector>

#include <msgpack.hpp>

#include <cocaine/common.hpp>
#include <cocaine/rpc/asio/decoder.hpp>

using namespace cocaine::framework::detail;

class decoded_message::impl {
public:
    impl() {}

    impl(const char* data, size_t size) {
        storage.resize(size);
        std::copy(data, data + size, storage.begin());

        size_t offset = 0;
        msgpack::object object;
        std::auto_ptr<msgpack::zone> zone(new msgpack::zone);
        msgpack::unpack(storage.data(), storage.size(), &offset, zone.get(), &object);

        unpacked.zone() = zone;
        unpacked.get() = object;
    }

    std::vector<char> storage;
    msgpack::unpacked unpacked;
};

decoded_message::decoded_message(boost::none_t) :
    d(new impl)
{}

decoded_message::decoded_message(const char* data, size_t size) :
    d(new impl(data, size))
{}

decoded_message::~decoded_message() {}

decoded_message::decoded_message(decoded_message&& other) :
    d(std::move(other.d))
{}

decoded_message& decoded_message::operator=(decoded_message&& other) {
    d = std::move(other.d);
    return *this;
}

auto decoded_message::span() const -> uint64_t {
    return d->unpacked.get().via.array.ptr[0].as<uint64_t>();
}

auto decoded_message::type() const -> uint64_t {
    return d->unpacked.get().via.array.ptr[1].as<uint64_t>();
}

auto decoded_message::args() const -> const msgpack::object& {
    return d->unpacked.get().via.array.ptr[2];
}


size_t decoder_t::decode(const char* data, size_t size, message_type& message, std::error_code& ec) {
    size_t offset = 0;

    msgpack::object object;
    std::auto_ptr<msgpack::zone> zone(new msgpack::zone);
    msgpack::unpack_return rv = msgpack::unpack(data, size, &offset, zone.get(), &object);

    if(rv == msgpack::UNPACK_SUCCESS || rv == msgpack::UNPACK_EXTRA_BYTES) {
        if (object.type != msgpack::type::ARRAY || object.via.array.size < 3) {
            ec = error::frame_format_error;
        } else if(object.via.array.ptr[0].type != msgpack::type::POSITIVE_INTEGER ||
                  object.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER ||
                  object.via.array.ptr[2].type != msgpack::type::ARRAY)
        {
            ec = error::frame_format_error;
        } else {
            message = message_type(data, offset);
        }
    } else if(rv == msgpack::UNPACK_CONTINUE) {
        ec = error::insufficient_bytes;
    } else if(rv == msgpack::UNPACK_PARSE_ERROR) {
        ec = error::parse_error;
    }

    return offset;
}
