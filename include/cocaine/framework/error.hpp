#pragma once

#include <stdexcept>
#include <string>
#include <tuple>

namespace cocaine {

namespace framework {

class cocaine_error : public std::runtime_error {
    int id;
    std::string reason;

public:
    explicit cocaine_error(const std::tuple<int, std::string>& err);
    cocaine_error(int id, const std::string& reason);

    ~cocaine_error() throw();
};

} // namespace framework

} // namespace cocaine
