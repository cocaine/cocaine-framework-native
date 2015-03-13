#pragma once

#include <stdexcept>
#include <string>
#include <tuple>

#include <cocaine/format.hpp>

namespace cocaine {

namespace framework {

class error_t : public std::runtime_error {
public:
    const int id;
    const std::string reason;

    explicit error_t(const std::tuple<int, std::string>& err);

    ~error_t() throw();
};

class service_not_found_error : public error_t {
public:
    service_not_found_error() :
        error_t(std::make_tuple(1, "service is not available"))
    {}
};

class version_mismatch_error : public error_t {
public:
    version_mismatch_error(int expected, int actual) :
        error_t(std::make_tuple(2, cocaine::format("version mismatch (%d expected, but %d actual)", expected, actual)))
    {}
};

} // namespace framework

} // namespace cocaine
