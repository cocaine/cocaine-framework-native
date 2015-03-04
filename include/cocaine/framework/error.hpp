#pragma once

#include <stdexcept>
#include <string>
#include <tuple>

namespace cocaine {

namespace framework {

class cocaine_error : public std::runtime_error {
public:
    const int id;
    const std::string reason;

    explicit cocaine_error(const std::tuple<int, std::string>& err);

    ~cocaine_error() throw();
};

class service_not_found_error : public cocaine_error {
public:
    service_not_found_error() :
        cocaine_error(std::make_tuple(1, "service is not available"))
    {}
};

} // namespace framework

} // namespace cocaine
