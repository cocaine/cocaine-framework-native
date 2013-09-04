#ifndef COCAINE_FRAMEWORK_BASIC_STREAM_HPP
#define COCAINE_FRAMEWORK_BASIC_STREAM_HPP

#include <cocaine/framework/common.hpp>

#include <string>
#include <exception>
#include <system_error>

namespace cocaine { namespace framework {

template<class T>
class basic_stream {
public:
    virtual
    void
    write(T&&) = 0;

    virtual
    void
    error(const std::exception_ptr& e) = 0;

    virtual
    void
    error(const std::system_error& err) {
        error(cocaine::framework::make_exception_ptr(err));
    }

    virtual
    void
    close() = 0;

    virtual
    bool
    closed() const = 0;

    virtual
    ~basic_stream() {
        // pass
    }
};

template<>
class basic_stream<void> {
public:
    virtual
    void
    error(const std::exception_ptr& e) = 0;

    virtual
    void
    error(const std::system_error& err) {
        error(cocaine::framework::make_exception_ptr(err));
    }

    virtual
    void
    close() = 0;

    virtual
    bool
    closed() const = 0;

    virtual
    ~basic_stream() {
        // pass
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_BASIC_STREAM_HPP
