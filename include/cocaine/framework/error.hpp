#pragma once

#include <stdexcept>
#include <string>
#include <tuple>

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

} // namespace framework

} // namespace cocaine
