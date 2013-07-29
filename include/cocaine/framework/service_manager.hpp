#ifndef COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP
#define COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP

#include <cocaine/framework/logging.hpp>

#include <cocaine/asio/reactor.hpp>
#include <cocaine/asio/tcp.hpp>

#include <memory>
#include <string>
#include <mutex>
#include <thread>

namespace cocaine { namespace framework {

class service_connection_t;

class service_manager_t :
    public std::enable_shared_from_this<service_manager_t>
{
    COCAINE_DECLARE_NONCOPYABLE(service_manager_t)

    friend class service_t;
    friend class service_connection_t;

public:
    typedef std::pair<std::string, uint16_t>
            endpoint_t;

public:
    static
    std::shared_ptr<service_manager_t>
    create(endpoint_t resolver_endpoint,
           const std::string& logging_prefix);

    static
    std::shared_ptr<service_manager_t>
    create(endpoint_t resolver_endpoint,
           std::shared_ptr<logger_t> logger = std::shared_ptr<logger_t>());

    ~service_manager_t();

    template<class Service, typename... Args>
    std::shared_ptr<Service>
    get_service(const std::string& name,
                Args&&... args)
    {
        return std::make_shared<Service>(this->get_connection(name, Service::version),
                                         std::forward<Args>(args)...);
    }

    template<class Service, typename... Args>
    std::shared_ptr<Service>
    get_service_async(const std::string& name,
                      Args&&... args)
    {
        return std::make_shared<Service>(this->get_connection_async(name, Service::version),
                                         std::forward<Args>(args)...);
    }

    std::shared_ptr<logger_t>
    get_system_logger() const;

private:
    service_manager_t(endpoint_t resolver_endpoint,
                      const executor_t& executor);

    void
    init();

    service_traits<cocaine::io::locator::resolve>::future_type
    resolve(const std::string& name);

    void
    register_connection(std::shared_ptr<service_connection_t> connection);

    void
    release_connection(std::shared_ptr<service_connection_t> connection);

    void
    remove_connection(std::shared_ptr<service_connection_t> connection);

    std::shared_ptr<service_connection_t>
    get_connection(const std::string& name,
                   int version);

    std::shared_ptr<service_connection_t>
    get_connection_async(const std::string& name,
                         int version);

private:
    cocaine::io::reactor_t m_ioservice;
    std::thread m_working_thread;
    endpoint_t m_resolver_endpoint;
    executor_t m_default_executor;

    std::set<std::shared_ptr<service_connection_t>> m_connections;
    std::mutex m_connections_lock;

    std::shared_ptr<service_connection_t> m_resolver;
    std::shared_ptr<logger_t> m_logger;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP
