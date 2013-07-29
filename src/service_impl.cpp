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
                                           unsigned int version) :
    m_name(name),
    m_version(version),
    m_manager(manager),
    m_connection_status(service_status::disconnected),
    m_dying(false),
    m_use_default_executor(true),
    m_session_counter(1)
{
    m_channel.reset(new iochannel_t);
}

service_connection_t::service_connection_t(const endpoint_t& endpoint,
                                           std::shared_ptr<service_manager_t> manager,
                                           unsigned int version) :
    m_endpoint(endpoint),
    m_version(version),
    m_manager(manager),
    m_connection_status(service_status::disconnected),
    m_dying(false),
    m_use_default_executor(true),
    m_session_counter(1)
{
    m_channel.reset(new iochannel_t);
}

service_connection_t::~service_connection_t() {
    reset_sessions();
}

std::shared_ptr<service_manager_t>
service_connection_t::manager() {
    auto m = m_manager.lock();
    if (m) {
        return m;
    } else {
        throw service_error_t(service_errc::broken_manager);
    }
}

std::string
service_connection_t::name() const {
    if (m_name) {
        return *m_name;
    } else {
        return cocaine::format("%s:%d", m_endpoint.first, m_endpoint.second);
    }
}

void
service_connection_t::soft_destroy() {
    std::unique_lock<std::recursive_mutex> lock(m_handlers_lock);

    if (!m_dying) {
        m_dying = true;

        if (m_handlers.empty() && m_connection_status != service_status::connecting) {
            auto m = m_manager.lock();
            if (m) {
                m->release_connection(shared_from_this());
            }
        }
    }
}

void
service_connection_t::disconnect(service_status status) {
    std::unique_lock<std::recursive_mutex> lock(m_handlers_lock);

    m_connection_status = status;
    std::shared_ptr<iochannel_t> channel(m_channel);
    m_channel.reset();
    reset_sessions();
    auto m = m_manager.lock();
    if (m) {
        m->m_ioservice.post(
            std::bind(&emptyf<std::shared_ptr<iochannel_t>, std::shared_ptr<service_connection_t>>::call,
                      channel,
                      shared_from_this())
        );

        if (m_dying) {
            m->release_connection(shared_from_this());
        }
    }
}

future<std::shared_ptr<service_connection_t>>
service_connection_t::reconnect() {
    std::unique_lock<std::recursive_mutex> lock(m_handlers_lock);

    if (m_connection_status == service_status::connecting) {
        throw service_error_t(service_errc::wait_for_connection);
    }

    disconnect();

    return connect();
}

future<std::shared_ptr<service_connection_t>>
service_connection_t::connect() {
    if (m_dying) {
        return make_ready_future<std::shared_ptr<service_connection_t>>
               ::error(service_error_t(service_errc::not_connected));
    }

    std::unique_lock<std::recursive_mutex> lock(m_handlers_lock);

    try {
        if (m_name) {
            auto m = manager();
            m_channel.reset(new iochannel_t);
            m_connection_status = service_status::connecting;
            return m->resolve(*m_name).then(std::bind(&service_connection_t::on_resolved,
                                                      shared_from_this(),
                                                      std::placeholders::_1));
        } else {
            m_channel.reset(new iochannel_t);
            m_connection_status = service_status::connecting;
            connect_to_endpoint();
            return make_ready_future<std::shared_ptr<service_connection_t>>::make(shared_from_this());
        }
    } catch (...) {
        if (m_connection_status == service_status::connecting) {
            disconnect();
        }
        return make_ready_future<std::shared_ptr<service_connection_t>>::error(std::current_exception());
    }
}

std::shared_ptr<service_connection_t>
service_connection_t::on_resolved(service_traits<cocaine::io::locator::resolve>::future_type& f) {
    try {
        auto service_info = f.next();

        if (m_version != std::get<1>(service_info)) {
            throw service_error_t(service_errc::bad_version);
        }

        std::tie(m_endpoint.first, m_endpoint.second) = std::get<0>(service_info);

        connect_to_endpoint();
    } catch (const service_error_t& e) {
        if (e.code().category() == service_response_category()) {
            disconnect(service_status::not_found);
            throw service_error_t(service_errc::not_found);
        } else {
            disconnect();
            throw;
        }
    } catch (...) {
        disconnect();
        throw;
    }

    return shared_from_this();
}

void
service_connection_t::connect_to_endpoint() {
    std::unique_lock<std::recursive_mutex> lock(m_handlers_lock);

    if (m_connection_status == service_status::connecting) {
        auto m = manager();

        auto endpoints = cocaine::io::resolver<cocaine::io::tcp>::query(m_endpoint.first, m_endpoint.second);

        std::exception_ptr e;

        for (size_t i = 0; i < endpoints.size(); ++i) {
            e = std::exception_ptr();
            try {
                auto socket = std::make_shared<cocaine::io::socket<cocaine::io::tcp>>(endpoints[i]);

                m_channel->attach(m->m_ioservice, socket);
                m_channel->rd->bind(std::bind(&service_connection_t::on_message,
                                              this,
                                              std::placeholders::_1),
                                    std::bind(&service_connection_t::on_error,
                                              this,
                                              std::placeholders::_1));
                m_channel->wr->bind(std::bind(&service_connection_t::on_error,
                                              this,
                                              std::placeholders::_1));
                m->m_ioservice.post(&emptyf<>::call); // wake up event-loop

                m_connection_status = service_status::connected;
            } catch (...) {
                e = std::current_exception();
                continue;
            }
            break;
        }

        if (e != std::exception_ptr()) {
            std::rethrow_exception(e);
        }
    }
}

void
service_connection_t::reset_sessions() {
    std::unique_lock<std::recursive_mutex> lock(m_handlers_lock);

    handlers_map_t handlers;
    handlers.swap(m_handlers);

    service_errc errc;

    if (m_connection_status == service_status::not_found) {
        errc = service_errc::not_found;
    } else {
        errc = service_errc::not_connected;
    }

    service_error_t err(errc);

    for (auto it = handlers.begin(); it != handlers.end(); ++it) {
        try {
            it->second->error(cocaine::framework::make_exception_ptr(err));
        } catch (...) {
            // optimize it
        }
    }
}

service_connection_t::session_id_t
service_connection_t::create_session(
    std::shared_ptr<detail::service::service_handler_concept_t> handler,
    std::shared_ptr<iochannel_t>& channel
)
{
    std::unique_lock<std::recursive_mutex> lock(m_handlers_lock);

    if (!m_dying &&
        (m_connection_status == service_status::disconnected ||
         m_connection_status == service_status::not_found))
    {
        reconnect();
    }

    session_id_t current_session;

    if (m_dying || (m_connection_status == service_status::disconnected)) {
        throw service_error_t(service_errc::not_connected);
    } else if (m_connection_status == service_status::not_found) {
        throw service_error_t(service_errc::not_found);
    } else {
        current_session = m_session_counter++;
        m_handlers[current_session] = handler;
        channel = m_channel;
    }
    return current_session;
}

void
service_connection_t::on_error(const std::error_code& /* code */) {
    try {
        reconnect();
    } catch (...) {
        // pass
    }
}

void
service_connection_t::on_message(const cocaine::io::message_t& message) {
    std::unique_lock<std::recursive_mutex> lock(m_handlers_lock);
    handlers_map_t::iterator it = m_handlers.find(message.band());

    if (it == m_handlers.end()) {
        auto m = m_manager.lock();
        if (m && m->get_system_logger()) {
            COCAINE_LOG_DEBUG(
                m->get_system_logger(),
                "Message with unknown session id has been received from service %s",
                name()
            );
        }
    } else {
        try {
            auto h = it->second;
            if (message.id() == io::event_traits<io::rpc::choke>::id) {
                m_handlers.erase(it);
            }
            if (m_dying && m_handlers.empty()) {
                disconnect();
            }
            lock.unlock();
            h->handle_message(message);
        } catch (const std::exception& e) {
            auto m = m_manager.lock();
            if (m && m->get_system_logger()) {
                COCAINE_LOG_WARNING(
                    m->get_system_logger(),
                    "Following error has occurred while handling message from service %s: %s",
                    name(),
                    e.what()
                );
            }
        }
    }
}
