#pragma once

#include <string>

#include <asio/io_service.hpp>

namespace cocaine {

namespace framework {

using loop_t = asio::io_service;

template<class T>
class service {
    // TODO: Static Assert - T contains protocol tag.

public:
    service(std::string name, loop_t& loop) noexcept {}

    bool connected() const noexcept {
        return false;
    }
};

} // namespace framework

} // namespace cocaine
