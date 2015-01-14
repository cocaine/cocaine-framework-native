#include "cocaine/rpc/protocol.hpp"

#include <gmock/gmock.h>

namespace cocaine {

namespace io {

struct mock_tag;

struct mock {

struct list {
    typedef mock_tag tag;

    static const char* alias() {
        return "list";
    }

    typedef option_of<int>::tag upstream_type;
};

};

} // namespace io

} // namespace cocaine
