#ifndef COCAINE_FRAMEWORK_COMMON_HPP
#define COCAINE_FRAMEWORK_COMMON_HPP

#include <stdexcept>
#include <exception>

namespace cocaine { namespace framework {

class socket_error_t :
    public std::runtime_error
{
public:
    explicit socket_error_t(const std::string& what) :
        std::runtime_error(what)
    {
        // pass
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_COMMON_HPP
