#include <cocaine/asio/resolver.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/common.hpp>
#include <cocaine/framework/services/logger.hpp>

#include <cocaine/messages.hpp>

using namespace cocaine::framework;

namespace {

    struct service_client_category_impl :
        public std::error_category
    {
        const char*
        name() const {
            return "service client";
        }

        std::string
        message(int error_value) const {
            switch (error_value) {
                case static_cast<int>(cocaine::framework::service_errc::bad_version):
                    return "Bad version of service";

                case static_cast<int>(cocaine::framework::service_errc::not_connected):
                    return "There is no connection to service";

                default:
                    return "Unknown service error o_O";
            }
        }
    };

    struct service_response_category_impl :
        public std::error_category
    {
        const char*
        name() const {
            return "service response";
        }

        std::string
        message(int /* error_value */) const {
            return "Error from service";
        }
    };

    service_client_category_impl client_category;
    service_response_category_impl response_category;

} // namespace

const std::error_category&
cocaine::framework::service_client_category() {
    return client_category;
}

const std::error_category&
cocaine::framework::service_response_category() {
    return response_category;
}

std::error_code
cocaine::framework::make_error_code(cocaine::framework::service_errc e) {
    return std::error_code(static_cast<int>(e), cocaine::framework::service_client_category());
}

std::error_condition
cocaine::framework::make_error_condition(cocaine::framework::service_errc e) {
    return std::error_condition(static_cast<int>(e), cocaine::framework::service_client_category());
}



service_t::service_t(const std::string& name,
                     service_manager_t& manager,
                     unsigned int version) :
    m_name(name),
    m_version(version),
    m_manager(manager),
    m_session_counter(1)
{
    // pass
}

service_t::service_t(const endpoint_t& endpoint,
                     service_manager_t& manager,
                     unsigned int version) :
    m_endpoint(endpoint),
    m_version(version),
    m_manager(manager),
    m_session_counter(1)
{
    // pass
}

void
service_t::reconnect() {
    if (m_name) {
        auto f = m_manager.async_resolve(*m_name);
        f.wait();
        on_resolved(f);
    } else {
        connect_to_endpoint();
    }
}

future<std::shared_ptr<service_t>>
service_t::reconnect_async() {
    if (m_name) {
        return this->m_manager.async_resolve(*m_name)
               .then(std::bind(&service_t::on_resolved, shared_from_this(), std::placeholders::_1));
    } else {
        connect_to_endpoint();
        return make_ready_future<std::shared_ptr<service_t>>::make(shared_from_this());
    }
}

std::shared_ptr<service_t>
service_t::on_resolved(service_t::handler<cocaine::io::locator::resolve>::future& f) {
    auto service_info = f.get();
    std::string hostname;
    uint16_t port;

    std::tie(hostname, port) = std::get<0>(service_info);

    m_endpoint = cocaine::io::resolver<cocaine::io::tcp>::query(hostname, port);

    if (m_version != std::get<1>(service_info)) {
        throw service_error_t(service_errc::bad_version);
    }

    connect_to_endpoint();

    return shared_from_this();
}

void
service_t::connect_to_endpoint() {
    if (!m_channel) {
        auto socket = std::make_shared<cocaine::io::socket<cocaine::io::tcp>>(m_endpoint);

        m_channel.reset(new iochannel_t(m_manager.m_ioservice, socket));
        m_channel->rd->bind(std::bind(&service_t::on_message, this, std::placeholders::_1),
                            std::bind(&service_t::on_error, this, std::placeholders::_1));
        m_channel->wr->bind(std::bind(&service_t::on_error, this, std::placeholders::_1));
    }
}

void
service_t::on_error(const std::error_code& /* code */) {
    m_channel.reset();

    handlers_map_t handlers;
    {
        std::lock_guard<std::mutex> lock(m_handlers_lock);
        handlers.swap(m_handlers);
    }

    std::exception_ptr error;
    try {
        throw service_error_t(service_errc::not_connected);
    } catch (...) {
        error = std::current_exception();
    }

    for (auto it = handlers.begin(); it != handlers.end(); ++it) {
        it->second->error(error);
    }
}

void
service_t::on_message(const cocaine::io::message_t& message) {
    handlers_map_t::iterator it;

    {
        std::lock_guard<std::mutex> lock(m_handlers_lock);
        it = m_handlers.find(message.band());
    }

    std::cout << "on_message in " << name() << " with id " << message.band() << std::endl;

    if (it == m_handlers.end()) {
        if (m_manager.get_system_logger()) {
            COCAINE_LOG_DEBUG(
                m_manager.get_system_logger(),
                "Message with unknown session id has been received from service %s",
                name()
            );
        }
    } else {
        try {
            it->second->handle_message(message);
        } catch (const std::exception& e) {
            if (m_manager.get_system_logger()) {
                COCAINE_LOG_WARNING(
                    m_manager.get_system_logger(),
                    "Following error has occurred while handling message from service %s: %s",
                    name(),
                    e.what()
                );
            }
        }

        if (message.id() == io::event_traits<io::rpc::choke>::id) {
            std::lock_guard<std::mutex> lock(m_handlers_lock);
            m_handlers.erase(it);
        }
    }
}


service_manager_t::service_manager_t(endpoint_t resolver_endpoint,
                                     const std::string& logging_prefix,
                                     const executor_t& executor) :
    m_resolver_endpoint(resolver_endpoint),
    m_default_executor(executor)
{
    // The order of initialization of objects is important here.
    // reactor_t must have at least one watcher before run (m_resolver is watcher),
    // but logging_service can be constructed only with m_ioservice running in thread.
    m_resolver.reset(
        new service_t(m_resolver_endpoint,
                      *this,
                      cocaine::io::protocol<cocaine::io::locator_tag>::version::value)
    );

    m_resolver->reconnect();

    m_working_thread = std::thread(&cocaine::io::reactor_t::run, &m_ioservice);

    std::shared_ptr<service_t> logging_service(new service_t("logging", *this, logging_service_t::version));
    logging_service->reconnect();
    m_logger = std::make_shared<logging_service_t>(logging_service, logging_prefix);
}

service_manager_t::~service_manager_t() {
    m_ioservice.stop();
    if (m_working_thread.joinable()) {
        m_working_thread.join();
    }
}
