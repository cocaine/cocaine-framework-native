#include "cocaine/framework/error.hpp"

using namespace cocaine::framework;

cocaine_error::cocaine_error(const std::tuple<int, std::string>& err) :
    std::runtime_error(std::get<1>(err)),
    id(std::get<0>(err)),
    reason(std::get<1>(err))
{}

cocaine_error::cocaine_error(int id, const std::string& reason) :
    std::runtime_error(reason),
    id(id),
    reason(reason)
{}

cocaine_error::~cocaine_error() throw() {}
