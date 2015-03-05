#include "cocaine/framework/message.hpp"

#include <vector>

#include <msgpack.hpp>

using namespace cocaine::framework;

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
