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
    
    template<class F>
    void
    execute(F&& callback) {
        m_reactor.post(std::forward<F>(callback));
    }
    
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

    // Create service and connect it synchronously.
    template<class Service, typename... Args>
    std::shared_ptr<Service>
    get_service(const std::string& name,
                Args&&... args)
    {
        return std::make_shared<Service>(this->get_connection(name, Service::version),
                                         std::forward<Args>(args)...);
    }

    // Make connection asynchronously and create service when the connection is established.
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

    // Just returns the logger used by the manager.
    const std::shared_ptr<logger_t>&
    get_system_logger() const;

    // Resolve service by name.
    service_traits<cocaine::io::locator::resolve>::future_type
    resolve(const std::string& name);

    // Returns a number of connections managed by the manager at the moment.
    size_t
    connections_count() const;

    // Returns a number of sessions created in all connections.
    // This and the next methods iterate over all connections, so don't call them every millisecond. They are slow.
    size_t
    sessions_count() const;

    // Returns amount of memory allocated by internal buffers of connections.
    size_t
    footprint() const;

private:
    service_manager_t();

    void
    init(const std::vector<endpoint_t>& resolver_endpoints, size_t threads_num);
    
    // Selects the next thread that will run a new connection.
    size_t
    next_reactor();

    // Make connection synchronously. This method throws an error if it fails to connect to service.
    std::shared_ptr<service_connection_t>
    get_connection(const std::string& name,
                   int version);

    // Make connection asynchronously. Doesn't throw exceptions.
    future<std::shared_ptr<service_connection_t>>
    get_connection_async(const std::string& name,
                         int version);

    // Yet another get_connection*. Returns a connection object immediately, but the object connects in background.
    // User could call connection returned by this method immediately, all requests will be sent when the object becomes connected.
    std::shared_ptr<service_connection_t>
    get_deferred_connection(const std::string& name,
                            int version);

    // Creates service from deferred connection. It's hidden from users for now, but service manager creates logger with this method.
    template<class Service, class... Args>
    std::shared_ptr<Service>
    get_deferred_service(const std::string& name,
                         Args&&... args)
    {
        return std::make_shared<Service>(this->get_deferred_connection(name, Service::version),
                                         std::forward<Args>(args)...);
    }

    // Utility method that is used as continuation of future and creates a service object from connection when the connection becomes ready.
    template<class Service, class... Args>
    std::shared_ptr<Service>
    make_service(future<std::shared_ptr<service_connection_t>>& connection,
                 Args&&... args)
    {
        return std::make_shared<Service>(connection.get(),
                                         std::forward<Args>(args)...);
    }

    // We need a shared pointer to create weak_ptr. Get_connection* methods register connections by this method.
    void
    register_connection(const std::shared_ptr<service_connection_t>& connection);

    // Connection calls this method in destructor, so it can provide only pointer (not shared_ptr).
    void
    release_connection(service_connection_t *connection);

private:
    // Pool of threads to run service clients.
    std::vector<std::unique_ptr<reactor_thread_t>> m_reactors;
    std::atomic<size_t> m_next_reactor;

    struct connection_pair_t {
        service_connection_t *connection;
        std::weak_ptr<service_connection_t> weak;

        bool
        operator==(const connection_pair_t& other) const {
            return connection == other.connection;
        }

        bool
        operator<(const connection_pair_t& other) const {
            return connection < other.connection;
        }
    };

    // Set of all connections managed by the service manager. It's needed to compute statistical information and for disconnecting all the connections when the manager dies.
    std::set<connection_pair_t> m_connections;
    mutable std::mutex m_connections_mutex;

    // Services used by the manager.
    std::shared_ptr<service_connection_t> m_resolver;
    std::shared_ptr<logger_t> m_logger;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_MANAGER_HPP
