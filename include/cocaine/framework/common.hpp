#ifndef COCAINE_FRAMEWORK_COMMON_HPP
#define COCAINE_FRAMEWORK_COMMON_HPP

#include <cocaine/platform.hpp>
#include <cocaine/traits.hpp>
#include <cocaine/common.hpp>

#include <msgpack.hpp>

#include <stdexcept>
#include <exception>
#include <utility>

#if defined(__clang__) || defined(HAVE_GCC46)
    #include <atomic>
#else
    #include <cstdatomic>
#endif

namespace cocaine { namespace framework {

template<class Exception>
std::exception_ptr
make_exception_ptr(Exception&& e) {
    try {
        throw std::forward<Exception>(e);
    } catch (...) {
        return std::current_exception();
    }
}

template<class T>
T&&
declval();

template<class T>
void
unpack(const char* data, size_t size, T& obj) {
    msgpack::unpacked msg;
    msgpack::unpack(&msg, data, size);
    cocaine::io::type_traits<T>::unpack(msg.get(), obj);
}

template<class T>
T
unpack(const char* data, size_t size) {
    msgpack::unpacked msg;
    msgpack::unpack(&msg, data, size);
    T obj;
    cocaine::io::type_traits<T>::unpack(msg.get(), obj);
    return obj;
}

template<class T>
void
unpack(const std::string& data, T& obj) {
    msgpack::unpacked msg;
    msgpack::unpack(&msg, data.data(), data.size());
    cocaine::io::type_traits<T>::unpack(msg.get(), obj);
}

template<class T>
T
unpack(const std::string& data) {
    msgpack::unpacked msg;
    msgpack::unpack(&msg, data.data(), data.size());
    T obj;
    cocaine::io::type_traits<T>::unpack(msg.get(), obj);
    return obj;
}

}} // namespace cocaine::framework


#endif // COCAINE_FRAMEWORK_COMMON_HPP
