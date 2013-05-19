#include <cocaine/asio/resolver.hpp>

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/common.hpp>

#include <cocaine/messages.hpp>

using namespace cocaine::framework;

service_t::service_t(const std::string& name,
                     cocaine::io::reactor_t& ioservice,
                     const cocaine::io::tcp::endpoint& resolver_endpoint,
                     std::shared_ptr<logger_t> logger,
                     unsigned int version) :
    m_name(name),
    m_version(version),
    m_ioservice(ioservice),
    m_logger(logger),
    m_session_counter(1)
{
    m_resolver.reset(new resolver_t(resolver_endpoint));
    connect();
}

void
service_t::connect() {
    auto service_info = m_resolver->resolve(m_name);

    std::string hostname;
    uint16_t port;

    std::tie(hostname, port) = std::get<0>(service_info);

    auto resolved = cocaine::io::resolver<cocaine::io::tcp>::query(hostname, port);

    m_endpoint = { resolved.address(), resolved.port() };

    if (m_version != std::get<1>(service_info)) {
        throw service_error_t("bad version of service " + m_name);
    }

    auto socket = std::make_shared<cocaine::io::socket<cocaine::io::tcp>>(resolved);

    m_channel.reset(new iochannel_t(m_ioservice, socket));
    m_channel->rd->bind(std::bind(&service_t::on_message, this, std::placeholders::_1),
                        std::bind(&service_t::on_error, this, std::placeholders::_1));
    m_channel->wr->bind(std::bind(&service_t::on_error, this, std::placeholders::_1));
}

void
service_t::on_error(const std::error_code& code) {
    throw socket_error_t(
        cocaine::format("socket error with code %d in service %s", code, m_name)
    );
}

void
service_t::on_message(const cocaine::io::message_t& message) {
    handlers_map_t::iterator it;

    {
        std::lock_guard<std::mutex> lock(m_handlers_lock);
        it = m_handlers.find(message.band());
    }

    if (it == m_handlers.end()) {
        COCAINE_LOG_WARNING(
            m_logger,
            "Message with unknown session id has been received from service %s",
            m_name
        );
    } else if (message.id() == io::event_traits<io::rpc::choke>::id) {
        std::lock_guard<std::mutex> lock(m_handlers_lock);
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
