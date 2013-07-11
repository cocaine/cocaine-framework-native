#ifndef COCAINE_FRAMEWORK_SERVICES_APP_HPP
#define COCAINE_FRAMEWORK_SERVICES_APP_HPP

#include <cocaine/framework/service.hpp>

#include <cocaine/traits/literal.hpp>

namespace cocaine { namespace framework {

struct app_service_t :
    public service_t
{
    static const unsigned int version = io::protocol<io::app_tag>::version::value;

    app_service_t(std::shared_ptr<service_connection_t> connection) :
        service_t(connection)
    {
        // Pass.
    }

    template<class T>
    service_traits<io::app::enqueue>::future_type
    enqueue(const std::string& event, const T& chunk) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> packer(buffer);

        io::type_traits<T>::pack(packer, chunk);

        return call<io::app::enqueue>(event, io::literal { buffer.data(), buffer.size() });
    }

    template<class T>
    service_traits<io::app::enqueue>::future_type
    enqueue(const std::string& event, const T& chunk, const std::string& tag) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> packer(buffer);

        io::type_traits<T>::pack(packer, chunk);

        return call<io::app::enqueue>(event, io::literal { buffer.data(), buffer.size() }, tag);
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_APP_HPP
