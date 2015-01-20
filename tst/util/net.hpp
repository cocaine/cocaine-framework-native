#pragma once

#include <cstdint>

/// Alias for asyncronous i/o implementation namespace (either boost::asio or pure asio).
namespace io = asio;

namespace testing {

namespace util {

static const std::uint64_t TIMEOUT = 1000;

/// An OS should select available port for us.
std::uint16_t port();

} // namespace util

} // namespace testing
