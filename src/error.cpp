#include "cocaine/framework/error.hpp"

using namespace cocaine::framework;

error_t::error_t(const std::tuple<int, std::string>& err) :
    std::runtime_error(std::get<1>(err)),
    id(std::get<0>(err)),
    reason(std::get<1>(err))
{}

error_t::~error_t() throw() {}
