#include <cocaine/framework/service.hpp>

#include <cocaine/messages.hpp>

using namespace cocaine::framework;

resolver_t::resolver_t(const cocaine::io::tcp::endpoint& endpoint) :
    m_channel(m_ioservice, std::make_shared<cocaine::io::socket<cocaine::io::tcp>>(endpoint))
{
    m_channel.rd->bind(std::bind(&resolver_t::on_response, this, std::placeholders::_1),
                       std::bind(&resolver_t::on_error, this, std::placeholders::_1));
}

const resolver_t::result_type&
resolver_t::resolve(const std::string& service_name) {
    m_error_flag = false;

    m_channel.wr->write<cocaine::io::locator::resolve>(0ul, service_name);

    m_ioservice.run();

    if (m_error_flag) {
        throw resolver_error_t(m_last_error.message, m_last_error.code);
    }

    return m_last_response;
}

void
resolver_t::on_response(const cocaine::io::message_t& message) {
    if (message.id() == io::event_traits<io::rpc::chunk>::id) {
        std::string data;
        message.as<io::rpc::chunk>(data);
        msgpack::unpacked msg;
        msgpack::unpack(&msg, data.data(), data.size());
        cocaine::io::type_traits<result_type>::unpack(msg.get(), m_last_response);
    } else if (message.id() == io::event_traits<io::rpc::choke>::id) {
        m_ioservice.native().unloop(ev::ALL);
    } else if (message.id() == io::event_traits<io::rpc::error>::id) {
        m_error_flag = true;
        message.as<io::rpc::error>(m_last_error.code, m_last_error.message);
    } else {
        m_error_flag = true;
        m_last_error.code = cocaine::error_code::invocation_error;
        m_last_error.message = "Message with unknown id was received.";
        m_ioservice.native().unloop(ev::ALL);
    }
}

void
resolver_t::on_error(const std::error_code&) {
    m_error_flag = true;
    m_last_error.code = cocaine::error_code::invocation_error;
    m_last_error.message = "Socket error has occurred in resolver.";
    m_ioservice.native().unloop(ev::ALL);
}
