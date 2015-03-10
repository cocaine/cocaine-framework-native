#pragma once

#include <cstdint>

#include <boost/lexical_cast.hpp>

namespace testing {

namespace util {

template<typename T>
static
T get_option(const char* name, T def) {
    if (char* value = ::getenv(name)) {
        return boost::lexical_cast<T>(value);
    }

    return def;
}

} // namespace util

} // namespace testing
