#ifndef COCAINE_FRAMEWORK_SERVICE_HPP
#define COCAINE_FRAMEWORK_SERVICE_HPP

#include <memory>
#include <string>
#include <boost/utility.hpp>

#include <cocaine/rpc/channel.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/asio/tcp.hpp>
#include <cocaine/asio/reactor.hpp>

namespace cocaine { namespace framework {

struct stub :
    public boost::noncopyable
{
    stub(const cocaine::io::tcp::endpoint& endpoint,
         cocaine::io::reactor_t& ioservice) :
        m_channel(ioservice, std::make_shared<cocaine::io::socket<cocaine::io::tcp>>(endpoint))
    {
        // pass
    }

    template<class Event, typename... Args>
    void
    call(Args&&... args) {
        m_channel.wr->write<Event>(0ul, args...);
    }

    template<class MessageHandler, class ErrorHandler>
    void
    bind(MessageHandler message_handler,
         ErrorHandler error_handler)
    {
        m_channel.rd->bind(message_handler, error_handler);
    }

private:
    cocaine::io::channel<
        cocaine::io::socket<cocaine::io::tcp>
    > m_channel;
};

struct service_manager_t :
    public boost::noncopyable
{
    service_manager_t(cocaine::io::reactor_t& ioservice) :
        m_ioservice(ioservice)
    {
        // pass
    }

    template<class ServiceT, class EndpointT = cocaine::io::tcp::endpoint>
    std::shared_ptr<ServiceT>
    get_service(const cocaine::io::tcp::endpoint& endpoint) {
        return std::shared_ptr<ServiceT>(new ServiceT(endpoint, m_ioservice));
    }

private:
    cocaine::io::reactor_t& m_ioservice;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_HPP
