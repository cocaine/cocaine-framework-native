#ifndef COCAINE_FRAMEWORK_FUTURE_ERROR_HPP
#define COCAINE_FRAMEWORK_FUTURE_ERROR_HPP

#include <system_error>
#include <stdexcept>
#include <exception>
#include <type_traits>

namespace cocaine { namespace framework {

enum class future_errc {
    broken_promise,
    future_already_retrieved,
    promise_already_satisfied,
    no_state
};

const std::error_category&
future_category();

std::error_code
make_error_code(future_errc e);

std::error_condition
make_error_condition(future_errc e);

struct future_error :
    public std::logic_error
{
    future_error (std::error_code ec) :
        std::logic_error(ec.message()),
        m_ec(ec)
    {
        // pass
    }

    const std::error_code&
    code() const {
        return m_ec;
    }

private:
    std::error_code m_ec;
};

}} // namespace cocaine::framework

namespace std {

template<>
struct is_error_code_enum<cocaine::framework::future_errc> :
    public true_type
{
    // pass
};

} // namespace std

#endif /* COCAINE_FRAMEWORK_FUTURE_ERROR_HPP */
