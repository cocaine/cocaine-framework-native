#ifndef COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP
#define COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/services/logger.hpp>

#include <thread>

namespace cocaine { namespace framework {

class service_manager_t {
    COCAINE_DECLARE_NONCOPYABLE(service_manager_t)

    friend class worker_t;

    typedef cocaine::io::tcp::endpoint endpoint_t;

public:
    service_manager_t(const std::string& logging_prefix,
                      uint16_t resolver_port,
                      const executor_t& executor) :
        m_default_executor(executor),
        m_resolver_port(resolver_port)
    {
        m_logger = std::make_shared<logging_service_t>(
                       "logging",
                       m_working_ioservice,
                       m_default_executor,
                       endpoint_t("127.0.0.1", m_resolver_port),
                       std::shared_ptr<logging_service_t>(),
                       logging_prefix
                   );

        std::static_pointer_cast<logging_service_t>(m_logger)->initialize();

        start();
    }

    ~service_manager_t() {
        m_working_ioservice.stop();
        if (m_working_thread.joinable()) {
            m_working_thread.join();
        }
    }

    template<class Service, typename... Args>
    std::shared_ptr<Service>
    get_service(const std::string& name,
                Args&&... args)
    {
        auto new_service = std::make_shared<Service>(name,
                                                     m_working_ioservice,
                                                     m_default_executor,
                                                     endpoint_t("127.0.0.1", m_resolver_port),
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
    void
    start() {
        if (!m_working_thread.joinable()) {
            m_working_thread = std::thread(&cocaine::io::reactor_t::run, &m_working_ioservice);
        }
    }

private:
    cocaine::io::reactor_t m_working_ioservice;
    executor_t m_default_executor;
    std::thread m_working_thread;
    uint16_t m_resolver_port;
    std::shared_ptr<logger_t> m_logger;
};

}} // cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP
