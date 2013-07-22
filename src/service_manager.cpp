#include <cocaine/framework/service.hpp>
#include <cocaine/framework/services/logger.hpp>

#include <cocaine/messages.hpp>

using namespace cocaine::framework;

std::shared_ptr<service_manager_t>
service_manager_t::create(endpoint_t resolver_endpoint,
       const std::string& logging_prefix,
       const executor_t& executor)
{
    std::shared_ptr<service_manager_t> manager(
        new service_manager_t(resolver_endpoint, executor)
    );
    manager->init();
    manager->m_logger = manager->get_service<logging_service_t>("logging", logging_prefix);
    return manager;
}

std::shared_ptr<service_manager_t>
service_manager_t::create(endpoint_t resolver_endpoint,
       std::shared_ptr<logger_t> logger,
       const executor_t& executor)
{
    std::shared_ptr<service_manager_t> manager(
        new service_manager_t(resolver_endpoint, executor)
    );
    manager->init();
    manager->m_logger = logger;
    return manager;
}

service_manager_t::service_manager_t(endpoint_t resolver_endpoint,
                                     const executor_t& executor) :
    m_resolver_endpoint(resolver_endpoint),
    m_default_executor(executor)
{
    // pass
}

service_manager_t::~service_manager_t() {
    stop();
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

    m_resolver->use_default_executor(false);
    std::unique_lock<std::recursive_mutex> lock(m_resolver->m_handlers_lock);
    m_resolver->connect(lock).get();

    m_working_thread = std::thread(&cocaine::io::reactor_t::run, &m_ioservice);
}

void
service_manager_t::stop() {
    if (m_working_thread.joinable()) {
        m_ioservice.post(std::bind(&cocaine::io::reactor_t::stop, &m_ioservice));
        m_working_thread.join();
    }
}

std::shared_ptr<logger_t>
service_manager_t::get_system_logger() const {
    return m_logger;
}

service_traits<cocaine::io::locator::resolve>::future_type
service_manager_t::resolve(const std::string& name) {
    if (m_resolver->status() == service_status::disconnected) {
        m_resolver->reconnect();
    }
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
    std::unique_lock<std::recursive_mutex> lock(service->m_handlers_lock);
    service->connect(lock).get();
    return service;
}

std::shared_ptr<service_connection_t>
service_manager_t::get_connection_async(const std::string& name,
                                        int version)
{
    std::shared_ptr<service_connection_t> service(
        new service_connection_t(name, shared_from_this(), version)
    );
    std::unique_lock<std::recursive_mutex> lock(service->m_handlers_lock);
    service->connect(lock);
    return service;
}
