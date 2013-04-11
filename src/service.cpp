#include <cocaine/framework/service.hpp>
#include <cocaine/framework/common.hpp>

#include <cocaine/messages.hpp>

using namespace cocaine::framework;

service_t::service_t(const std::string& name,
                     cocaine::io::reactor_t& ioservice,
                     const cocaine::io::tcp::endpoint& resolver_endpoint,
                     std::shared_ptr<logging_service_t> logger,
                     int version) :
    m_name(name),
    m_version(version),
    m_ioservice(ioservice),
    m_session_counter(1),
    m_logger(logger)
{
    m_resolver.reset(new resolver_t(resolver_endpoint));
    connect();
}

void
service_t::connect() {
    auto service_info = m_resolver->resolve(m_name);

    std::tie(m_endpoint.first, m_endpoint.second) = std::get<0>(service_info);

    if (m_version != std::get<1>(service_info)) {
        throw service_error_t("bad version of service " + m_name);
    }

    auto socket = std::make_shared<cocaine::io::socket<cocaine::io::tcp>>(
        cocaine::io::tcp::endpoint(m_endpoint.first, m_endpoint.second)
    );

    m_channel.reset(new iochannel_t(m_ioservice, socket));
    m_channel->rd->bind(std::bind(&service_t::on_message, this, std::placeholders::_1),
                        std::bind(&service_t::on_error, this, std::placeholders::_1));
}

void
service_t::on_error(const std::error_code& code) {
    throw socket_error_t(
        cocaine::format("socket error with code %d in service %s", code, m_name)
    );
}

void
service_t::on_message(const cocaine::io::message_t& message) {
    std::lock_guard<std::mutex> lock(m_handlers_lock);

    auto it = m_handlers.find(message.band());

    if (it == m_handlers.end()) {
        COCAINE_LOG_WARNING(
            m_logger,
            "Message with unknown session id has been received from service %s",
            m_name
        );
    } else if (message.id() == io::event_traits<io::rpc::choke>::id) {
        m_handlers.erase(it);
    } else {
        try {
            it->second->handle_message(message);
        } catch (const std::exception& e) {
            COCAINE_LOG_ERROR(
                m_logger,
                "Following error has occurred while handling message from service %s: %s",
                name(),
                e.what()
            );
        }
    }
}
