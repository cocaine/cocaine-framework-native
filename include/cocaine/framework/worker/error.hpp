#pragma once

#include <string>
#include <system_error>
#include <tuple>

#include "cocaine/framework/error.hpp"

namespace cocaine {

namespace framework {

namespace worker {

namespace error {

/// Worker specific error codes.
enum worker_errors {
    /// The worker was timeouted to hear from the runtime a heartbeat message.
    disowned,
    /// The worker has been explicitly terminated by the runtime.
    terminated,
    /// The worker receives a message with unknown type.
    invalid_protocol_type,
    /// The runtime has unexpectedly closed the channel.
    unexpected_eof,
};

/// Request specific error codes.
enum request_errors {
    /// Unspecified error from the client with its own error code and description.
    unspecified,
};

/// Identifies the worker error category by returning an const lvalue reference to it.
const std::error_category& worker_category();

/// Identifies the client request error category by returning an const lvalue reference to it.
const std::error_category& request_category();

} // namespace error

/*!
 * The disowned error is thrown when the Framework gives up to hear a heartbeat message from the
 * runtime.
 *
 * \param timeout specified the timeout in seconds, that was waited before throwing the exception.
 *
 * Normally, the internal event loop should properly catch this exception and transform it to the
 * error code.
 */
class disowned_error : public error_t {
    int timeout_;

public:
    explicit disowned_error(int timeout);

    /// Returns the number of seconds, for which the worker was waited.
    int timeout() const noexcept;
};

/*!
 * The termination error is thrown when the Framework received termination protocol message from the
 * runtime.
 */
class termination_error : public error_t {
    std::error_code ec;

public:
    explicit termination_error(const std::error_code& ec);

    /// Returns the actual reason for termination, which is set by the runtime.
    std::error_code error_code() const noexcept;
};

/*!
 * The invalid protocol type error is thrown when the Framework receives a protocol message with
 * unknown type.
 *
 * Normally you should never receive such kind of error. If you do, usually it points that you are
 * using Framework's version non-compatible with the runtime.
 */
class invalid_protocol_type : public error_t {
    std::uint64_t type_;

public:
    explicit invalid_protocol_type(std::uint64_t type);

    /// Returns invalid type number.
    std::uint64_t type() const noexcept;
};

/*!
 * The exception class, that is thrown by the Framework when it receives protocol message with
 * client's error.
 */
class request_error : public error_t {
    int id_;

public:
    request_error(int id, std::string reason);

    /// Returns the request errno.
    int id() const noexcept;
};

class unexpected_eof : public error_t {
public:
    unexpected_eof();
};

} // namespace worker

} // namespace framework

} // namespace cocaine

namespace std {

template<>
struct is_error_code_enum<cocaine::framework::worker::error::worker_errors> : public true_type {};

template<>
struct is_error_condition_enum<cocaine::framework::worker::error::worker_errors> : public true_type {};

} // namespace std
