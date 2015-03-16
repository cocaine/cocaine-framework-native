#include "cocaine/framework/worker/error.hpp"

#include <cocaine/format.hpp>

/// Extended description formatting patterns.
static const char ERROR_DISOWNED[]       = "disowning due to timeout expiration (%d s)";
static const char ERROR_TERMINATED[]     = "terminating due to: %s";
static const char ERROR_INVALID_TYPE[]   = "received a message with invalid type (%d)";
static const char ERROR_UNEXPECTED_EOF[] = "the runtime has unexpectedly closed the clannel";

using namespace cocaine::framework::worker;

namespace {

struct worker_category_t : public std::error_category {
    const char*
    name() const noexcept {
        return "worker category";
    }

    std::string
    message(int err) const noexcept {
        switch (err) {
        case static_cast<int>(error::disowned):
            return "the worker was timeouted to hear from the runtime a heartbeat message";
        case static_cast<int>(error::terminated):
            return "the worker is explicitly terminated by the runtime";
        case static_cast<int>(error::invalid_protocol_type):
            return "the worker has received a protocol message with invalid type";
        default:
            return "unexpected worker error";
        }
    }
};

struct request_category_t : public std::error_category {
    const char*
    name() const noexcept {
        return "worker request category";
    }

    std::string
    message(int) const noexcept {
        return "error from the client";
    }
};

} // namespace

const std::error_category& error::worker_category() {
    static worker_category_t category;
    return category;
}

const std::error_category& error::request_category() {
    static request_category_t category;
    return category;
}

std::error_code
make_error_code(error::worker_errors err) {
    return std::error_code(static_cast<int>(err), error::worker_category());
}

std::error_code
make_error_code(error::request_errors err) {
    return std::error_code(static_cast<int>(err), error::request_category());
}

disowned_error::disowned_error(int timeout) :
    error_t(make_error_code(error::disowned), cocaine::format(ERROR_DISOWNED, timeout)),
    timeout_(timeout)
{}

int disowned_error::timeout() const noexcept {
    return timeout_;
}

termination_error::termination_error(const std::error_code& ec) :
    error_t(make_error_code(error::terminated), cocaine::format(ERROR_TERMINATED, ec.message())),
    ec(ec)
{}

std::error_code termination_error::error_code() const noexcept {
    return ec;
}

invalid_protocol_type::invalid_protocol_type(std::uint64_t type) :
    error_t(make_error_code(error::invalid_protocol_type), cocaine::format(ERROR_INVALID_TYPE, type)),
    type_(type)
{}

std::uint64_t invalid_protocol_type::type() const noexcept {
    return type_;
}

request_error::request_error(int id, std::string description) :
    error_t(make_error_code(error::unspecified), std::move(description)),
    id_(id)
{}

int request_error::id() const noexcept {
    return id_;
}

unexpected_eof::unexpected_eof() :
    error_t(make_error_code(error::unexpected_eof), ERROR_UNEXPECTED_EOF)
{}
