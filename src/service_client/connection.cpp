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

#include <cocaine/asio/resolver.hpp>

using namespace cocaine::framework;

namespace {
    template<class... Args>
    struct emptyf {
        static
        void
        call(Args...){
            // pass
        }
    };
}

service_connection_t::service_connection_t(const std::string& name,
                                           std::shared_ptr<service_manager_t> manager,
                                           cocaine::io::reactor_t& reactor,
                                           unsigned int version) :
    m_name(name),
    m_version(version),
    m_session_counter(1),
    m_manager(manager),
    m_reactor(reactor),
    m_connection_status(service_status::disconnected),
    m_auto_reconnect(true)
{
    // pass
}

service_connection_t::service_connection_t(const std::vector<endpoint_t>& endpoints,
                                           std::shared_ptr<service_manager_t> manager,
                                           cocaine::io::reactor_t& reactor,
                                           unsigned int version) :
    m_endpoints(endpoints),
    m_version(version),
    m_session_counter(1),
    m_manager(manager),
    m_reactor(reactor),
    m_connection_status(service_status::disconnected),
    m_auto_reconnect(true)
{
    // pass
}

service_connection_t::~service_connection_t() {
    auto m = get_manager();
    if (m) {
        m->release_connection(this);
    }
}

std::shared_ptr<service_manager_t>
service_connection_t::get_manager() const {
    return m_manager.lock();
}

std::string
service_connection_t::name() const {
    if (m_name) {
        return *m_name;
    } else if (!m_endpoints.empty()) {
        return cocaine::format("%s:%d", m_endpoints[0].first, m_endpoints[0].second);
    } else {
        return "<uninitialized>";
    }
}

void
service_connection_t::auto_reconnect(bool reconnect) {
    m_auto_reconnect = reconnect;
}
    
cocaine::io::reactor_t&
service_connection_t::reactor() const {
    return m_reactor;
}

size_t
service_connection_t::footprint() const {
    std::unique_lock<std::mutex> lock(m_sessions_mutex);
    return m_channel.footprint();
}

size_t
service_connection_t::sessions_count() const {
    std::unique_lock<std::mutex> lock(m_sessions_mutex);
    return m_sessions.size();
}

void
service_connection_t::disconnect(service_status status) {
    sessions_map_t sessions;

    {
        std::unique_lock<std::mutex> lock(m_sessions_mutex);

        m_connection_status = status;
        sessions.swap(m_sessions);
        m_channel = iochannel_t();
    }

    service_errc errc;

    if (status == service_status::not_found) {
        errc = service_errc::not_found;
    } else {
        errc = service_errc::not_connected;
    }

    service_error_t err(errc);

    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        try {
            it->second.handler()->error(cocaine::framework::make_exception_ptr(err));
        } catch (const std::exception& e) {
            auto m = get_manager();
            if (m && m->get_system_logger()) {
                COCAINE_LOG_DEBUG(m->get_system_logger(),
                                  "Following error has occurred while handling disconnect: %s",
                                  e.what());
            }
        }
    }
}

future<std::shared_ptr<service_connection_t>>
service_connection_t::connect() {
    auto m = get_manager();

    if (!m) {
        return make_ready_future<std::shared_ptr<service_connection_t>>
               ::error(service_error_t(service_errc::broken_manager));
    } else if (m_name) {
        m_connection_status = service_status::connecting;
        return m->resolve(*m_name).then(std::bind(&service_connection_t::on_resolved,
                                                  shared_from_this(),
                                                  std::placeholders::_1));
    } else {
        m_connection_status = service_status::connecting;
        packaged_task<std::shared_ptr<service_connection_t>()> connector(
            std::bind(&service_connection_t::connect_to_endpoint,
                      shared_from_this())
        );
        m_reactor.post(connector);
        return connector.get_future();
    }
}

future<std::shared_ptr<service_connection_t>>
service_connection_t::on_resolved(service_traits<cocaine::io::locator::resolve>::future_type& f) {
    try {
        auto service_info = f.next();

        if (m_version != std::get<1>(service_info)) {
            throw service_error_t(service_errc::bad_version);
        }

        m_endpoints.resize(1);
        std::tie(m_endpoints[0].first, m_endpoints[0].second) = std::get<0>(service_info);
    } catch (const service_error_t& e) {
        if (e.code().category() == service_response_category()) {
            m_reactor.post(std::bind(&service_connection_t::disconnect,
                                     shared_from_this(),
                                     service_status::not_found));
            throw service_error_t(service_errc::not_found);
        } else {
            m_reactor.post(std::bind(&service_connection_t::disconnect,
                                     shared_from_this(),
                                     service_status::disconnected));
            throw;
        }
    } catch (...) {
        m_reactor.post(std::bind(&service_connection_t::disconnect,
                                 shared_from_this(),
                                 service_status::disconnected));
        throw;
    }

    packaged_task<std::shared_ptr<service_connection_t>()> connector(
        std::bind(&service_connection_t::connect_to_endpoint, shared_from_this())
    );
    m_reactor.post(connector);

    return connector.get_future();
}

std::shared_ptr<service_connection_t>
service_connection_t::connect_to_endpoint() {
    auto m = get_manager();

    if (!m) {
        disconnect();
        throw service_error_t(service_errc::broken_manager);
    }

    std::exception_ptr err;

    std::unique_lock<std::mutex> lock(m_sessions_mutex);

    // iterate over hosts provided by user or by locator
    for (size_t hostidx = 0;
        hostidx < m_endpoints.size() && m_connection_status == service_status::connecting;
        ++hostidx)
    {
        try {
            // resolve hostname
            auto endpoints = cocaine::io::resolver<cocaine::io::tcp>::query(
                m_endpoints[hostidx].first,
                m_endpoints[hostidx].second
            );

            // iterate over IP's returned by DNS server
            for (size_t i = 0; i < endpoints.size(); ++i) {
                try {
                    auto socket = std::make_shared<cocaine::io::socket<cocaine::io::tcp>>(endpoints[i]);

                    m_channel.attach(m_reactor, socket);
                    m_channel.rd->bind(std::bind(&service_connection_t::on_message,
                                                 this,
                                                 std::placeholders::_1),
                                       std::bind(&service_connection_t::on_error,
                                                 this,
                                                 std::placeholders::_1));
                    m_channel.wr->bind(std::bind(&service_connection_t::on_error,
                                                 this,
                                                 std::placeholders::_1));

                    m_connection_status = service_status::connected;
                    return shared_from_this();
                } catch (...) {
                    err = std::current_exception();
                    continue;
                }
            }
        } catch (...) {
            err = std::current_exception();
            continue;
        }
    }

    lock.unlock();

    disconnect();

    if (err == std::exception_ptr()) {
        throw service_error_t(service_errc::not_connected);
    } else {
        std::rethrow_exception(err);
    }
}

void
service_connection_t::on_error(const std::error_code& /* code */) {
    auto self = shared_from_this(); // keep the connection alive while the method is running

    disconnect();

    if (m_auto_reconnect) {
        std::unique_lock<std::mutex> lock(m_sessions_mutex);
        connect();
    }

    if (self.unique()) {
        m_reactor.post(std::bind(&emptyf<std::shared_ptr<service_connection_t>>::call, self));
    }
}

void
service_connection_t::on_message(const cocaine::io::message_t& message) {
    auto self = shared_from_this(); // keep the connection alive while the method is running

    std::unique_lock<std::mutex> lock(m_sessions_mutex);
    sessions_map_t::iterator it = m_sessions.find(message.band());

    if (it != m_sessions.end()) {
        it->second.stop_timer();
        detail::service::session_data_t data;
        if (message.id() == io::event_traits<io::rpc::choke>::id) {
            m_sessions.erase(it);
        } else if (message.id() == io::event_traits<io::rpc::error>::id) {
            data = it->second;
            m_sessions.erase(it);
        } else {
            data = it->second;
        }

        lock.unlock();

        if (data.handler()) {
            try {
                data.handler()->handle_message(message);
            } catch (const std::exception& e) {
                auto m = get_manager();
                if (m && m->get_system_logger()) {
                    COCAINE_LOG_DEBUG(m->get_system_logger(),
                                      "Error has occurred while processing a message from service '%s': %s",
                                      name(),
                                      e.what());
                }
            }
        }
    }

    if (self.unique()) {
        m_reactor.post(std::bind(&emptyf<std::shared_ptr<service_connection_t>>::call, self));
    }
}

void
service_connection_t::delete_session(session_id_t id,
                                     service_errc ec)
{
    auto self = shared_from_this(); // keep the connection alive while the method is running

    detail::service::session_data_t s;

    {
        std::unique_lock<std::mutex> lock(m_sessions_mutex);
        sessions_map_t::iterator it = m_sessions.find(id);
        if (it != m_sessions.end()) {
            s = it->second;
            m_sessions.erase(it);
        }
    }

    if (s.handler()) {
        try {
            s.handler()->error(cocaine::framework::make_exception_ptr(service_error_t(ec)));
        } catch (const std::exception& e) {
            auto m = get_manager();
            if (m && m->get_system_logger()) {
                COCAINE_LOG_DEBUG(m->get_system_logger(),
                                  "Following error has occurred while handling timeout: %s",
                                  e.what());
            }
        }
    }

    if (self.unique()) {
        m_reactor.post(std::bind(&emptyf<std::shared_ptr<service_connection_t>>::call, self));
    }
}

void
service_connection_t::set_timeout(session_id_t id,
                                  float seconds)
{
    auto m = get_manager();

    if (m) {
        m_reactor.post(std::bind(&service_connection_t::set_timeout_impl,
                                 shared_from_this(),
                                 id,
                                 seconds));
    }
}

void
service_connection_t::set_timeout_impl(session_id_t id,
                                       float seconds)
{
    std::unique_lock<std::mutex> lock(m_sessions_mutex);

    auto it = m_sessions.find(id);
    if (it != m_sessions.end()) {
        it->second.set_timeout(seconds);
    }
}


detail::service::session_data_t::session_data_t() :
    m_stopped(false)
{
    // pass
}

detail::service::session_data_t::session_data_t(
    const std::shared_ptr<service_connection_t>& connection,
    session_id_t id,
    std::shared_ptr<detail::service::service_handler_concept_t>&& handler
) :
    m_id(id),
    m_handler(std::move(handler)),
    m_stopped(false),
    m_connection(connection)
{
    // pass
}

detail::service::session_data_t::~session_data_t() {
    stop_timer();
}

void
detail::service::session_data_t::set_timeout(float seconds) {
    if (!m_stopped) {
        auto m = m_connection->get_manager();
        if (m) {
            m_close_timer = std::make_shared<ev::timer>(m_connection->reactor().native());
            m_close_timer->set<detail::service::session_data_t,
                               &detail::service::session_data_t::on_timeout>(this);
            m_close_timer->priority = EV_MINPRI;
            m_close_timer->start(seconds);
        }
    }
}

void
detail::service::session_data_t::stop_timer() {
    if (m_close_timer) {
        m_close_timer->stop();
        m_close_timer.reset();
    }
    m_stopped = true; // forbid setting of timeout after timer is stopped
}

void
detail::service::session_data_t::on_timeout(ev::timer&, int) {
    m_connection->delete_session(m_id, service_errc::timeout);
}
