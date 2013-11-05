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

#ifndef COCAINE_FRAMEWORK_SERVICE_CONNECTION_HPP
#define COCAINE_FRAMEWORK_SERVICE_CONNECTION_HPP

#include <cocaine/framework/service_client/session.hpp>
#include <cocaine/framework/service_client/status.hpp>

#include <cocaine/asio/tcp.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/rpc/channel.hpp>

#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>

namespace cocaine { namespace framework {

class service_connection_t;

namespace detail { namespace service {

    class session_data_t {
    public:
        session_data_t();

        session_data_t(const std::shared_ptr<service_connection_t>& connection,
                       session_id_t id,
                       std::shared_ptr<detail::service::service_handler_concept_t>&& handler);

        ~session_data_t();

        void
        set_timeout(float seconds);

        void
        stop_timer();

        detail::service::service_handler_concept_t*
        handler() const {
            return m_handler.get();
        }

    private:
        void
        on_timeout(ev::timer&, int);

    private:
        session_id_t m_id;
        std::shared_ptr<detail::service::service_handler_concept_t> m_handler;
        std::shared_ptr<ev::timer> m_close_timer;
        bool m_stopped;
        std::shared_ptr<service_connection_t> m_connection;
    };

}} // namespace detail::service

class service_manager_t;

class service_connection_t :
    public std::enable_shared_from_this<service_connection_t>
{
    COCAINE_DECLARE_NONCOPYABLE(service_connection_t)

public:
    typedef std::pair<std::string, uint16_t>
            endpoint_t;

public:
    service_connection_t(const std::string& name,
                         std::shared_ptr<service_manager_t> manager,
                         cocaine::io::reactor_t& reactor,
                         unsigned int version);

    service_connection_t(const std::vector<endpoint_t>& endpoints,
                         std::shared_ptr<service_manager_t> manager,
                         cocaine::io::reactor_t& reactor,
                         unsigned int version);

    // If the connection owns some IO watcher and corresponding event loop is running, then the connection must be deleted only from the event loop.
    ~service_connection_t();

    // Returns name of the service or one of the endpoints if the service has no name.
    std::string
    name() const;

    int
    version() const {
        return m_version;
    }

    service_status
    status() const {
        return m_connection_status;
    }

    cocaine::io::reactor_t&
    reactor() const;

    void
    auto_reconnect(bool);

    std::shared_ptr<service_manager_t>
    get_manager() const;

    template<class Event, typename... Args>
    session<Event>
    call(Args&&... args);

    // Call it only when disconnected!
    future<std::shared_ptr<service_connection_t>>
    connect();

    // Must be called only from service thread if the thread is running.
    void
    disconnect(service_status status = service_status::disconnected);

    void
    delete_session(session_id_t id, service_errc ec);

    void
    set_timeout(session_id_t id, float seconds);

    size_t
    footprint() const;

    size_t
    sessions_count() const;

private:
    std::shared_ptr<service_connection_t>
    connect_to_endpoint();

    future<std::shared_ptr<service_connection_t>>
    on_resolved(session<cocaine::io::locator::resolve>::future_type&);

    void
    on_error(const std::error_code&);

    void
    on_message(const cocaine::io::message_t& message);

    void
    set_timeout_impl(session_id_t id, float seconds);

private:
    typedef std::unordered_map<session_id_t, detail::service::session_data_t>
            sessions_map_t;

    typedef cocaine::io::channel<cocaine::io::socket<cocaine::io::tcp>>
            iochannel_t;

    boost::optional<std::string> m_name;
    std::vector<endpoint_t> m_endpoints;
    unsigned int m_version;

    std::atomic<session_id_t> m_session_counter;
    sessions_map_t m_sessions;
    mutable std::mutex m_sessions_mutex;

    std::weak_ptr<service_manager_t> m_manager;
    cocaine::io::reactor_t &m_reactor;
    iochannel_t m_channel;

    service_status m_connection_status;
    std::atomic<bool> m_auto_reconnect;
};

}} // namespace cocaine::framework

#include <cocaine/framework/service_client/connection.inl>
#include <cocaine/framework/service_client/session.inl>

#endif // COCAINE_FRAMEWORK_SERVICE_CONNECTION_HPP
