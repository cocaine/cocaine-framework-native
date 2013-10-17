#include <cocaine/framework/service.hpp>
#include <cocaine/framework/services/logger.hpp>

#include <cocaine/messages.hpp>

#ifdef __linux__
# include <sys/prctl.h>
#endif

using namespace cocaine::framework;

namespace {
    void
    set_name() {
#ifdef __linux__
        prctl(PR_SET_NAME, "service-manager");
#endif
    }
}

reactor_thread_t::reactor_thread_t() {
    execute(&set_name);
    m_thread = std::thread(&cocaine::io::reactor_t::run, &m_reactor);
}

void
reactor_thread_t::stop() {
    execute(std::bind(&cocaine::io::reactor_t::stop, &m_reactor));
}

void
reactor_thread_t::join() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void
reactor_thread_t::execute(const std::function<void()>& callback) {
    m_reactor.post(callback);
}

cocaine::io::reactor_t&
reactor_thread_t::reactor() {
    return m_reactor;
}

std::shared_ptr<service_manager_t>
service_manager_t::create(const std::vector<endpoint_t>& resolver_endpoints,
                          const std::string& logging_prefix,
                          size_t threads_num)
{
    std::shared_ptr<service_manager_t> manager(
        new service_manager_t(resolver_endpoints)
    );
    manager->init(threads_num);
    manager->m_logger = manager->get_deferred_service<logging_service_t>("logging", logging_prefix);
    return manager;
}

std::shared_ptr<service_manager_t>
service_manager_t::create(const std::vector<endpoint_t>& resolver_endpoints,
                          std::shared_ptr<logger_t> logger,
                          size_t threads_num)
{
    std::shared_ptr<service_manager_t> manager(
        new service_manager_t(resolver_endpoints)
    );
    manager->init(threads_num);
    manager->m_logger = logger;
    return manager;
}

service_manager_t::service_manager_t(const std::vector<endpoint_t>& resolver_endpoints) :
    m_next_reactor(0),
    m_resolver_endpoints(resolver_endpoints)
{
    // pass
}

service_manager_t::~service_manager_t() {
    for (size_t i = 0; i < m_reactors.size(); ++i) {
        m_reactors[i]->stop();
    }
    
    for (size_t i = 0; i < m_reactors.size(); ++i) {
        m_reactors[i]->join();
    }

    m_resolver->disconnect();

    std::unique_lock<std::mutex> lock(m_connections_lock);
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        (*it)->disconnect();
    }

    m_resolver.reset();
}

void
service_manager_t::init(size_t threads_num) {
    m_reactors.reserve(threads_num);
    
    for (size_t i = 0; i < threads_num; ++i) {
        m_reactors.emplace_back(new reactor_thread_t());
    }
    
    m_resolver = std::make_shared<service_connection_t>(
        m_resolver_endpoints,
        shared_from_this(),
        0,
        cocaine::io::protocol<cocaine::io::locator_tag>::version::value
    );

    m_resolver->connect();
    
}

std::shared_ptr<logger_t>
service_manager_t::get_system_logger() const {
    return m_logger;
}

service_traits<cocaine::io::locator::resolve>::future_type
service_manager_t::resolve(const std::string& name) {
    return std::move(m_resolver->call<cocaine::io::locator::resolve>(name).downstream());
}

void
service_manager_t::register_connection(service_connection_t *connection) {
    std::unique_lock<std::mutex> lock(m_connections_lock);
    m_connections.insert(connection);
}

void
service_manager_t::release_connection(service_connection_t *connection) {
    std::unique_lock<std::mutex> lock(m_connections_lock);
    m_connections.erase(connection);
}

size_t
service_manager_t::next_reactor() {
    return (m_next_reactor++) % m_reactors.size();
}

std::shared_ptr<service_connection_t>
service_manager_t::get_connection(const std::string& name,
                                  int version)
{
    auto service = std::make_shared<service_connection_t>(name,
                                                          shared_from_this(),
                                                          next_reactor(),
                                                          version);
    service->connect().get();
    return service;
}

future<std::shared_ptr<service_connection_t>>
service_manager_t::get_connection_async(const std::string& name,
                                        int version)
{
    auto service = std::make_shared<service_connection_t>(name,
                                                          shared_from_this(),
                                                          next_reactor(),
                                                          version);
    return service->connect();
}

std::shared_ptr<service_connection_t>
service_manager_t::get_deferred_connection(const std::string& name,
                                           int version)
{
    auto service = std::make_shared<service_connection_t>(name,
                                                          shared_from_this(),
                                                          next_reactor(),
                                                          version);
    service->connect();
    return service;
}

size_t
service_manager_t::footprint() const {
    size_t result = 0;

    std::unique_lock<std::mutex> lock(m_connections_lock);
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        result += (*it)->footprint();
    }

    return result;
}

size_t
service_manager_t::connections_count() const {
    std::unique_lock<std::mutex> lock(m_connections_lock);
    return m_connections.size();
}

size_t
service_manager_t::sessions_count() const {
    size_t result = 0;

    std::unique_lock<std::mutex> lock(m_connections_lock);
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        result += (*it)->sessions_count();
    }

    return result;
}
