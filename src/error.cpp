#include "cocaine/framework/error.hpp"

#include <cocaine/format.hpp>

using namespace cocaine::framework;

/// Extended description formatting patterns.
static const char ERROR_SERVICE_NOT_FOUND[] = "the service '%s' is not available";
static const char ERROR_VERSION_MISMATCH[]  = "version mismatch (%d expected, but %d actual)";

namespace {

struct service_category_t : public std::error_category {
    const char*
    name() const noexcept {
        return "service category";
    }

    std::string
    message(int err) const noexcept {
        switch (err) {
        case static_cast<int>(cocaine::framework::error::service_not_found):
            return "the specified service was not found in the locator";
        case static_cast<int>(cocaine::framework::error::version_mismatch):
            return "the service provides API with version different than required";
        default:
            return "unexpected service error";
        }
    }
};

struct response_category_t : public std::error_category {
    const char*
    name() const noexcept {
        return "service response category";
    }

    std::string
    message(int) const noexcept {
        return "error from the service";
    }
};

} // namespace

const std::error_category&
cocaine::framework::error::service_category() {
    static service_category_t category;
    return category;
}

const std::error_category&
cocaine::framework::error::response_category() {
    static response_category_t category;
    return category;
}

std::error_code
error::make_error_code(error::service_errors err) {
    return std::error_code(static_cast<int>(err), error::service_category());
}

std::error_condition
error::make_error_condition(error::service_errors err) {
    return std::error_condition(static_cast<int>(err), error::service_category());
}

std::error_code
error::make_error_code(error::response_errors err) {
    return std::error_code(static_cast<int>(err), error::response_category());
}

std::error_condition
error::make_error_condition(error::response_errors err) {
    return std::error_condition(static_cast<int>(err), error::response_category());
}

error_t::error_t(const std::error_code& ec, const std::string& description) :
    std::system_error(ec, description)
{}

error_t::~error_t() noexcept {}

service_not_found::service_not_found(const std::string& name) :
    error_t(error::service_not_found, cocaine::format(ERROR_SERVICE_NOT_FOUND, name)),
    name_(name)
{}

const std::string& service_not_found::name() const noexcept {
    return name_;
}

version_mismatch::version_mismatch(int expected, int actual) :
    error_t(error::version_mismatch, cocaine::format(ERROR_VERSION_MISMATCH, expected, actual)),
    expected_(expected),
    actual_(actual)
{}

int version_mismatch::expected() const noexcept {
    return expected_;
}

int version_mismatch::actual() const noexcept {
    return actual_;
}

response_error::response_error(std::tuple<int, std::string>& err) :
    error_t(error::unspecified, cocaine::format("[%d]: %s", std::get<0>(err), std::get<1>(err))),
    id_(std::get<0>(err))
{}

int response_error::id() const noexcept {
    return id_;
}
