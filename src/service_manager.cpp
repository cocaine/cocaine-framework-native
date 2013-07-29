#include <cocaine/framework/service.hpp>
#include <cocaine/framework/services/logger.hpp>

#include <cocaine/messages.hpp>

using namespace cocaine::framework;

std::shared_ptr<service_manager_t>
service_manager_t::create(endpoint_t resolver_endpoint,
                          const std::string& logging_prefix)
{
    std::shared_ptr<service_manager_t> manager(
        new service_manager_t(resolver_endpoint)
    );
    manager->init();
    manager->m_logger = manager->get_service_async<logging_service_t>("logging", logging_prefix);
    return manager;
}

std::shared_ptr<service_manager_t>
service_manager_t::create(endpoint_t resolver_endpoint,
                          std::shared_ptr<logger_t> logger)
{
    std::shared_ptr<service_manager_t> manager(
        new service_manager_t(resolver_endpoint)
    );
    manager->init();
    manager->m_logger = logger;
    return manager;
}

service_manager_t::service_manager_t(endpoint_t resolver_endpoint) :
    m_resolver_endpoint(resolver_endpoint)
{
    // pass
}

service_manager_t::~service_manager_t() {
    if (m_working_thread.joinable()) {
        m_ioservice.post(std::bind(&cocaine::io::reactor_t::stop, &m_ioservice));
        m_working_thread.join();
    }

    std::unique_lock<std::mutex> lock(m_connections_lock);
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        (*it)->disconnect();
    }

    m_resolver.reset();
}

void
service_manager_t::init() {
    // The order of initialization of objects is important here.
    // reactor_t must have at least one watcher before run (m_resolver is watcher).
    m_resolver.reset(
        new service_connection_t(m_resolver_endpoint,
                                 shared_from_this(),
                                 cocaine::io::protocol<cocaine::io::locator_tag>::version::value)
    );

    m_resolver->connect();

    m_working_thread = std::thread(&cocaine::io::reactor_t::run, &m_ioservice);
}

std::shared_ptr<logger_t>
service_manager_t::get_system_logger() const {
    return m_logger;
}

service_traits<cocaine::io::locator::resolve>::future_type
service_manager_t::resolve(const std::string& name) {
    return m_resolver->call<cocaine::io::locator::resolve>(name);
}

void
service_manager_t::register_connection(std::shared_ptr<service_connection_t> connection) {
    std::unique_lock<std::mutex> lock(m_connections_lock);
    m_connections.insert(connection);
}

void
service_manager_t::release_connection(std::shared_ptr<service_connection_t> connection) {
    m_ioservice.post(std::bind(&service_manager_t::remove_connection, this, connection));
}

void
service_manager_t::remove_connection(std::shared_ptr<service_connection_t> connection) {
    std::unique_lock<std::mutex> lock(m_connections_lock);
    m_connections.erase(connection);
}

std::shared_ptr<service_connection_t>
service_manager_t::get_connection(const std::string& name,
                                  int version)
{
    std::shared_ptr<service_connection_t> service(
        new service_connection_t(name, shared_from_this(), version)
    );
    service->connect().get();
    return service;
}

std::shared_ptr<service_connection_t>
service_manager_t::get_connection_async(const std::string& name,
                                        int version)
{
    std::shared_ptr<service_connection_t> service(
        new service_connection_t(name, shared_from_this(), version)
    );
    service->connect();
    return service;
}
