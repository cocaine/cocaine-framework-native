#ifndef COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP
#define COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP

#include <cocaine/framework/service_client/service.hpp>
#include <cocaine/framework/logging.hpp>

#include <cocaine/asio/reactor.hpp>
#include <cocaine/asio/tcp.hpp>
#include <cocaine/messages.hpp>

#include <memory>
#include <string>
#include <mutex>
#include <thread>

namespace cocaine { namespace framework {

class reactor_thread_t {
public:
    reactor_thread_t();
    
    void
    stop();
    
    void
    join();
    
    void
    execute(const std::function<void()>& callback);
    
    cocaine::io::reactor_t&
    reactor();
    
private:
    cocaine::io::reactor_t m_reactor;
    std::thread m_thread;
};

class service_manager_t :
    public std::enable_shared_from_this<service_manager_t>
{
    COCAINE_DECLARE_NONCOPYABLE(service_manager_t)

    friend class service_t;
    friend class service_connection_t;
    friend class detail::service::session_data_t;

public:
    typedef std::pair<std::string, uint16_t>
            endpoint_t;

public:
    static
    std::shared_ptr<service_manager_t>
    create(const std::vector<endpoint_t>& resolver_endpoints,
           const std::string& logging_prefix,
           size_t threads_num = 1);

    static
    std::shared_ptr<service_manager_t>
    create(const std::vector<endpoint_t>& resolver_endpoints,
           std::shared_ptr<logger_t> logger = std::shared_ptr<logger_t>(),
           size_t threads_num = 1);

    template<class... Args>
    static
    std::shared_ptr<service_manager_t>
    create(const endpoint_t& resolver,
           Args&&... args)
    {
        std::vector<endpoint_t> r;
        r.emplace_back(resolver);
        return create(r, std::forward<Args>(args)...);
    }

    ~service_manager_t();

    template<class Service, typename... Args>
    std::shared_ptr<Service>
    get_service(const std::string& name,
                Args&&... args)
    {
        return std::make_shared<Service>(this->get_connection(name, Service::version),
                                         std::forward<Args>(args)...);
    }

    template<class Service, class... Args>
    future<std::shared_ptr<Service>>
    get_service_async(const std::string& name,
                      Args&&... args)
    {
        return this->get_connection_async(name, Service::version)
            .then(std::bind(&service_manager_t::make_service<Service, Args...>,
                            this,
                            std::placeholders::_1,
                            std::forward<Args>(args)...));
    }

    std::shared_ptr<logger_t>
    get_system_logger() const;

    service_traits<cocaine::io::locator::resolve>::future_type
    resolve(const std::string& name);

private:
    service_manager_t(const std::vector<endpoint_t>& resolver_endpoints);

    void
    init(size_t threads_num);

    void
    register_connection(service_connection_t *connection);

    void
    release_connection(service_connection_t *connection);
    
    size_t
    next_reactor();

    std::shared_ptr<service_connection_t>
    get_connection(const std::string& name,
                   int version);

    future<std::shared_ptr<service_connection_t>>
    get_connection_async(const std::string& name,
                         int version);

    std::shared_ptr<service_connection_t>
    get_deferred_connection(const std::string& name,
                            int version);

    template<class Service, class... Args>
    std::shared_ptr<Service>
    get_deferred_service(const std::string& name,
                         Args&&... args)
    {
        return std::make_shared<Service>(this->get_deferred_connection(name, Service::version),
                                         std::forward<Args>(args)...);
    }

    template<class Service, class... Args>
    std::shared_ptr<Service>
    make_service(future<std::shared_ptr<service_connection_t>>& connection,
                 Args&&... args)
    {
        return std::make_shared<Service>(connection.get(),
                                         std::forward<Args>(args)...);
    }

private:
    std::vector<std::unique_ptr<reactor_thread_t>> m_reactors;
    std::atomic<size_t> m_next_reactor;

    std::set<service_connection_t*> m_connections;
    std::mutex m_connections_lock;

    std::vector<endpoint_t> m_resolver_endpoints;
    std::shared_ptr<service_connection_t> m_resolver;
    std::shared_ptr<logger_t> m_logger;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP
