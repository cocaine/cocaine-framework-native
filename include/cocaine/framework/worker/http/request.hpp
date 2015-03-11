#pragma once

#include <string>
#include <vector>

#include <cocaine/traits/tuple.hpp>

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
