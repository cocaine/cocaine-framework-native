#pragma once

#include <string>
#include <system_error>
#include <tuple>

/// This module provides access to client-side error codes and exceptions.
///
/// \unstable because it needs some user experience.

namespace cocaine {

namespace framework {

namespace error {

/// Service specific error codes.
enum service_errors {
    /// The specified service is not available.
    service_not_found = 1,
    /// The service provides API with version different than required.
    version_mismatch
};

/// Response specific error codes.
enum response_errors {
    /// Unspecified error from the service with its own error code and description.
    unspecified = 1
};

/// Identifies the service error category by returning an const lvalue reference to it.
const std::error_category& service_category();

/// Identifies the response error category by returning an const lvalue reference to it.
const std::error_category& response_category();

/*!
 * Constructs an `service_errors` error code.
 *
 * This function is called by the constructor of std::error_code when given an `service_errors`
 * argument.
 */
std::error_code make_error_code(service_errors err);

/*!
 * Constructs an `service_errors` error condition.
 *
 * This function is called by the constructor of std::error_condition when given an `service_errors`
 * argument.
 */
std::error_condition make_error_condition(service_errors err);

/*!
 * Constructs an `response_errors` error code.
 *
 * This function is called by the constructor of std::error_code when given an `response_errors`
 * argument.
 */
std::error_code make_error_code(response_errors err);

/*!
 * Constructs an `response_errors` error condition.
 *
 * This function is called by the constructor of std::error_condition when given
 * an `response_errors` argument.
 */
std::error_condition make_error_condition(response_errors err);

} // namespace error

/*!
 * The error class represents the root of Framework's error hierarchy.
 *
 * Use it if you want to handle all Framework's not associated with the network errors.
 */
class error_t : public std::system_error {
public:
    error_t(const std::error_code& ec, const std::string& description);

    ~error_t() noexcept;
};

/*!
 * The exception class, that is thrown by the Framework when it is unable to locate the specified
 * service.
 *
 * You can always obtain service's name by calling the corresponding method.
 */
class service_not_found : public error_t {
    std::string name_;

public:
    explicit service_not_found(const std::string& name);

    ~service_not_found() noexcept;

    /// Returns service's name, which was failed to locate.
    const std::string& name() const noexcept;
};

/*!
 * The exception class, that is thrown by the Framework when it detects protocol version mismatch.
 */
class version_mismatch : public error_t {
    int expected_;
    int actual_;

public:
    version_mismatch(int expected, int actual);

    /// Returns the desired protocol version number.
    int expected() const noexcept;

    /// Returns the actual protocol version number.
    int actual() const noexcept;
};

/*!
 * The exception class, that is thrown by the Framework when it detects unspecified service's error.
 */
class response_error : public error_t {
    const int id_;

public:
    explicit response_error(std::tuple<int, std::string>& err);

    /// Returns the response errno.
    int id() const noexcept;
};

} // namespace framework

} // namespace cocaine

namespace std {

/// Extends the type trait std::is_error_code_enum to identify `service_errors` error codes.
template<>
struct is_error_code_enum<cocaine::framework::error::service_errors> : public true_type {};

/// Extends the type trait std::is_error_condition_enum to identify `service_errors` error
/// conditions.
template<>
struct is_error_condition_enum<cocaine::framework::error::service_errors> : public true_type {};

/// Extends the type trait std::is_error_code_enum to identify `response_errors` error codes.
template<>
struct is_error_code_enum<cocaine::framework::error::response_errors> : public true_type {};

/// Extends the type trait std::is_error_condition_enum to identify `response_errors` error
/// conditions.
template<>
struct is_error_condition_enum<cocaine::framework::error::response_errors> : public true_type {};

} // namespace std
