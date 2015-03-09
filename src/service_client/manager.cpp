/*
Copyright (c) 2013 Andrey Goryachev <andrey.goryachev@gmail.com>
Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

This file is part of Cocaine.

Cocaine is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

Cocaine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

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

class service_manager_t::reactor_thread_t {
public:
    reactor_thread_t() {
        m_reactor.post(&set_name);
        m_thread = std::thread(&cocaine::io::reactor_t::run, &m_reactor);
    }

    ~reactor_thread_t() {
        if (m_thread.joinable()) {
            m_reactor.post(std::bind(&cocaine::io::reactor_t::stop, &m_reactor));
            m_thread.join();
        }
    }

    void
    stop() {
        m_reactor.post(std::bind(&cocaine::io::reactor_t::stop, &m_reactor));
    }

    void
    join() {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    cocaine::io::reactor_t&
    reactor() {
        return m_reactor;
    }

private:
    cocaine::io::reactor_t m_reactor;
    std::thread m_thread;
};

std::shared_ptr<service_manager_t>
service_manager_t::create(const std::vector<endpoint_t>& resolver_endpoints,
                          const std::string& logging_prefix,
                          size_t threads_num)
{
    std::shared_ptr<service_manager_t> manager(new service_manager_t);
    manager->init(resolver_endpoints, threads_num);
    manager->m_logger = manager->get_deferred_service<logging_service_t>("logging", logging_prefix);
    return manager;
}

std::shared_ptr<service_manager_t>
service_manager_t::create(const std::vector<endpoint_t>& resolver_endpoints,
                          std::shared_ptr<logger_t> logger,
                          size_t threads_num)
{
    std::shared_ptr<service_manager_t> manager(new service_manager_t);
    manager->init(resolver_endpoints, threads_num);
    manager->m_logger = logger;
    return manager;
}

service_manager_t::service_manager_t() :
    m_next_reactor(0)
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

    if (m_resolver) {
        m_resolver->auto_reconnect(false);
        m_resolver->disconnect();
    }

    {
        std::lock_guard<std::mutex> lock(m_connections_mutex);
        for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
            auto shared = it->weak.lock();
            if (shared) {
                shared->auto_reconnect(false);
                shared->disconnect();
            }
        }
    }

    m_reactors.clear();
    m_resolver.reset();
    m_logger.reset();
}

void
service_manager_t::init(const std::vector<endpoint_t>& resolver_endpoints, size_t threads_num) {
    m_reactors.reserve(threads_num);
    
    for (size_t i = 0; i < threads_num; ++i) {
        m_reactors.emplace_back(new reactor_thread_t());
    }
    
    m_resolver = std::make_shared<service_connection_t>(
        resolver_endpoints,
        shared_from_this(),
        m_reactors[0]->reactor(),
        cocaine::io::protocol<cocaine::io::locator_tag>::version::value
    );

    m_resolver->connect();
}

const std::shared_ptr<logger_t>&
service_manager_t::get_system_logger() const {
    return m_logger;
}

service_traits<cocaine::io::locator::resolve>::future_type
service_manager_t::resolve(const std::string& name) {
    auto session = m_resolver->call<cocaine::io::locator::resolve>(name);
    session.set_timeout(10.0f); // may be this timeout must be configurable?
    return std::move(session.downstream());
}

void
service_manager_t::register_connection(const std::shared_ptr<service_connection_t>& connection) {
    std::lock_guard<std::mutex> guard(m_connections_mutex);
    m_connections.insert(connection_pair_t {connection.get(),
                                            std::weak_ptr<service_connection_t>(connection)});
}

void
service_manager_t::release_connection(service_connection_t *connection) {
    std::lock_guard<std::mutex> guard(m_connections_mutex);
    m_connections.erase(connection_pair_t {connection, std::weak_ptr<service_connection_t>()});
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
                                                          m_reactors[next_reactor()]->reactor(),
                                                          version);
    register_connection(service);
    service->connect().get();
    return service;
}

future<std::shared_ptr<service_connection_t>>
service_manager_t::get_connection_async(const std::string& name,
                                        int version)
{
    auto service = std::make_shared<service_connection_t>(name,
                                                          shared_from_this(),
                                                          m_reactors[next_reactor()]->reactor(),
                                                          version);
    register_connection(service);
    return service->connect();
}

std::shared_ptr<service_connection_t>
service_manager_t::get_deferred_connection(const std::string& name,
                                           int version)
{
    auto service = std::make_shared<service_connection_t>(name,
                                                          shared_from_this(),
                                                          m_reactors[next_reactor()]->reactor(),
                                                          version);
    register_connection(service);
    service->connect();
    return service;
}

size_t
service_manager_t::footprint() const {
    size_t result = 0;

    std::unique_lock<std::mutex> lock(m_connections_mutex);
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        result += it->connection->footprint();
    }

    return result;
}

size_t
service_manager_t::connections_count() const {
    std::unique_lock<std::mutex> lock(m_connections_mutex);
    return m_connections.size();
}

size_t
service_manager_t::sessions_count() const {
    size_t result = 0;

    std::unique_lock<std::mutex> lock(m_connections_mutex);
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        result += it->connection->sessions_count();
    }

    return result;
}
