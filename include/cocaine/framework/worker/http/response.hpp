#pragma once

#include <string>
#include <vector>

#include <cocaine/traits/tuple.hpp>

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
