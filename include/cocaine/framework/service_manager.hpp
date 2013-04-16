#ifndef COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP
#define COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/services/logger.hpp>

namespace cocaine { namespace framework {

class service_manager_t {
    COCAINE_DECLARE_NONCOPYABLE(service_manager_t)

    typedef cocaine::io::tcp::endpoint endpoint_t;

public:
    service_manager_t(cocaine::io::reactor_t& ioservice,
                      const std::string& logging_prefix) :
        m_ioservice(ioservice)
    {
        m_logger = std::make_shared<logging_service_t>(
                       "logging",
                       m_ioservice,
                       endpoint_t("127.0.0.1", 10053),
                       std::shared_ptr<logging_service_t>(),
                       logging_prefix
                   );
    }

    template<class Service, typename... Args>
    std::shared_ptr<Service>
    get_service(const std::string& name,
                Args&&... args)
    {
        auto new_service = std::make_shared<Service>(name,
                                                     m_ioservice,
                                                     endpoint_t("127.0.0.1", 10053),
                                                     m_logger,
                                                     std::forward<Args>(args)...);
        new_service->initialize();
        return new_service;
    }

    std::shared_ptr<logger_t>
    get_system_logger() {
        return m_logger;
    }

private:
    cocaine::io::reactor_t& m_ioservice;

    std::shared_ptr<logging_service_t> m_logger;
};

}} // cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP
