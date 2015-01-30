#pragma once

#include <string>
#include <vector>

#include <cocaine/rpc/protocol.hpp>

namespace testing {

namespace mock {

struct event_tag;

struct event {
    struct void_dispatch_slot {
        typedef event_tag tag;

        static const char* alias() {
            return "void_dispatch_slot";
        }
    };

    struct streaming {
        typedef event_tag tag;

        static const char* alias() {
            return "streaming";
        }

        typedef cocaine::io::stream_of<
            std::string
        >::tag dispatch_type;
    };

    struct list {
        typedef event_tag tag;

        static const char* alias() {
            return "list";
        }

        typedef cocaine::io::option_of<
            std::vector<std::string>
        >::tag upstream_type;
    };
};

} // namespace mock

} // namespace testing

namespace cocaine { namespace io {

template<>
struct protocol<testing::mock::event_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        testing::mock::event::list,
        testing::mock::event::void_dispatch_slot,
        testing::mock::event::streaming
    > messages;

    typedef testing::mock::event scope;
};

}} // namespace cocaine::io
