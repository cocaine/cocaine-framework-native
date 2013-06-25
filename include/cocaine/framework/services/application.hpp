#ifndef COCAINE_FRAMEWORK_SERVICES_APPLICATION_HPP
#define COCAINE_FRAMEWORK_SERVICES_APPLICATION_HPP

#include <cocaine/framework/service.hpp>

namespace cocaine { namespace framework {

struct application_client_t :
    public service_stub_t
{
    static const unsigned int version = cocaine::io::protocol<cocaine::io::app_tag>::version::value;

    application_client_t(std::shared_ptr<service_t> service) :
        service_stub_t(service)
    {
        // pass
    }

    service_t::handler<cocaine::io::app::enqueue>::future
    enqueue(const std::string& event,
            const std::string& chunk)
    {
        return backend()->call<cocaine::io::app::enqueue>(event, chunk);
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_APPLICATION_HPP
