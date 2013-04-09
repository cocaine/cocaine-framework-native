#ifndef COCAINE_FRAMEWORK_STREAM_HPP
#define COCAINE_FRAMEWORK_STREAM_HPP

#include <cocaine/api/stream.hpp>

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
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_STREAM_HPP
