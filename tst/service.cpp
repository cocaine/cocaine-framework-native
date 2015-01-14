#include <type_traits>

#include <gtest/gtest.h>

#include <cocaine/idl/node.hpp>

#include <cocaine/framework/service.hpp>

#include "mock.hpp"

using namespace cocaine::framework;

TEST(Service, Constructor) {
    loop_t loop;
    service<cocaine::io::node> node("node", loop);
    EXPECT_FALSE(node.connected());

    static_assert(
        std::is_nothrow_constructible<service<cocaine::io::node>, std::string, loop_t&>::value,
        "service constructor must be noexcept"
    );
}

