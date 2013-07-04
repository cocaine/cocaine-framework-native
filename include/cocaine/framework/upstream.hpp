#ifndef COCAINE_FRAMEWORK_UPSTREAM_HPP
#define COCAINE_FRAMEWORK_UPSTREAM_HPP

#include <cocaine/api/stream.hpp>
#include <cocaine/traits.hpp>

namespace cocaine { namespace framework {

class upstream_t:
    public cocaine::api::stream_t
{
public:
    upstream_t()
    {
        // pass
    }

    virtual
    ~upstream_t() {
        // pass
    }

    virtual
    bool
    closed() const = 0;

    using cocaine::api::stream_t::write;

    template<class T>
    void
    write(const T& obj) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> packer(buffer);
        cocaine::io::type_traits<T>::pack(packer, obj);
        write(buffer.data(), buffer.size());
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_UPSTREAM_HPP
