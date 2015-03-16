#pragma once

#include <string>
#include <system_error>
#include <tuple>

#include "cocaine/framework/error.hpp"

/// This module provides access to worker-side error codes and exceptions.
///
/// \unstable because it needs some user experience.

namespace cocaine {

namespace framework {

namespace worker {

namespace error {

/// Worker specific error codes.
enum worker_errors {
    /// The worker was timeouted to hear from the runtime a heartbeat message.
    disowned = 1,
    /// The worker has been explicitly terminated by the runtime.
    terminated,
    /// The worker receives a message with unknown type.
    invalid_protocol_type,
    /// The runtime has unexpectedly closed the channel.
    unexpected_eof
};

/// Request specific error codes.
enum request_errors {
    /// Unspecified error from the client with its own error code and description.
    unspecified = 1
};

/// Identifies the worker error category by returning an const lvalue reference to it.
const std::error_category& worker_category();

/// Identifies the client request error category by returning an const lvalue reference to it.
const std::error_category& request_category();

/*!
 * Constructs an `worker_errors` error code.
 *
 * This function is called by the constructor of std::error_code when given an `worker_errors`
 * argument.
 */
std::error_code make_error_code(worker_errors err);

/*!
 * Constructs an `worker_errors` error condition.
 *
 * This function is called by the constructor of std::error_condition when given an `worker_errors`
 * argument.
 */
std::error_condition make_error_condition(worker_errors err);

/*!
 * Constructs an `request_errors` error code.
 *
 * This function is called by the constructor of std::error_code when given an `request_errors`
 * argument.
 */
std::error_code make_error_code(request_errors err);

/*!
 * Constructs an `request_errors` error condition.
 *
 * This function is called by the constructor of std::error_condition when given an `request_errors`
 * argument.
 */
std::error_condition make_error_condition(request_errors err);

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
public:
    termination_error();
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
 * The unexpected eof exception is thrown by the Framework when it detects, that the runtime has
 * unexpectedly closed the channel.
 *
 * It can happen, for example, when the user has been disconnected from the runtime for some
 * reasons.
 */
class unexpected_eof : public error_t {
public:
    unexpected_eof();
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

} // namespace worker

} // namespace framework

} // namespace cocaine

namespace std {

/// Extends the type trait std::is_error_code_enum to identify `worker_errors` error codes.
template<>
struct is_error_code_enum<cocaine::framework::worker::error::worker_errors> : public true_type {};

/// Extends the type trait std::is_error_condition_enum to identify `worker_errors` error
/// conditions.
template<>
struct is_error_condition_enum<cocaine::framework::worker::error::worker_errors> : public true_type {};

/// Extends the type trait std::is_error_code_enum to identify `request_errors` error codes.
template<>
struct is_error_code_enum<cocaine::framework::worker::error::request_errors> : public true_type {};

/// Extends the type trait std::is_error_condition_enum to identify `request_errors` error
/// conditions.
template<>
struct is_error_condition_enum<cocaine::framework::worker::error::request_errors> : public true_type {};

} // namespace std
