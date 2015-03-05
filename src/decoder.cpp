#include "cocaine/framework/detail/decoder.hpp"

#include <vector>

#include <msgpack.hpp>

#include <cocaine/common.hpp>
#include <cocaine/rpc/asio/decoder.hpp>

#include "cocaine/framework/message.hpp"

using namespace cocaine::framework::detail;

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
