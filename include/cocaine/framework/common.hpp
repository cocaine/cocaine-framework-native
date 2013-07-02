#ifndef COCAINE_FRAMEWORK_COMMON_HPP
#define COCAINE_FRAMEWORK_COMMON_HPP

#include <msgpack.hpp>
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

template<class T>
void
unpack(const char* data, size_t size, T& obj) {
    msgpack::unpacked msg;
    msgpack::unpack(&msg, data, size);
    msg.get().convert<T>(&obj);
}

template<class T>
T
unpack(const char* data, size_t size) {
    msgpack::unpacked msg;
    msgpack::unpack(&msg, data, size);
    return msg.get().as<T>();
}

template<class T>
void
unpack(const std::string& data, T& obj) {
    msgpack::unpacked msg;
    msgpack::unpack(&msg, data.data(), data.size());
    msg.get().convert<T>(&obj);
}

template<class T>
T
unpack(const std::string& data) {
    msgpack::unpacked msg;
    msgpack::unpack(&msg, data.data(), data.size());
    return msg.get().as<T>();
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_COMMON_HPP
