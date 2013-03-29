#ifndef COCAINE_FRAMEWORK_SERVICE_HPP
#define COCAINE_FRAMEWORK_SERVICE_HPP

#include <cocaine/detail/locator.hpp>
#include <cocaine/rpc/channel.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/asio/tcp.hpp>
#include <cocaine/asio/reactor.hpp>

#include <memory>
#include <string>
#include <stdexcept>
#include <boost/utility.hpp>

namespace cocaine { namespace framework {

class resolver_error_t :
    public std::runtime_error
{
public:
    explicit resolver_error_t(const std::string& what,
                              int code = -1) :
        runtime_error(what),
        m_code(code)
    {
        // pass
    }

    int
    code() const {
        return m_code;
    }

private:
    int m_code;
};

class resolver_t {
public:
    resolver_t(const cocaine::io::tcp::endpoint& endpoint);

    const cocaine::description_t&
    resolve(const std::string& service_name);

private:
    void
    on_response(const io::message_t& message);

    void
    on_error(const std::error_code&);

private:
    struct error_t {
        int code;
        std::string message;
    };

private:
    cocaine::io::reactor_t m_ioservice;
    cocaine::io::channel<
        cocaine::io::socket<cocaine::io::tcp>
    > m_channel;

    cocaine::description_t m_last_response;

    bool m_error_flag;
    error_t m_last_error;
};

class service_t :
    private boost::noncopyable
{
    typedef cocaine::io::channel<cocaine::io::socket<cocaine::io::tcp>>
            iochannel_t;
public:
    service_t(const cocaine::io::tcp::endpoint& endpoint,
              cocaine::io::reactor_t& ioservice);

    service_t(const std::string& name,
              cocaine::io::reactor_t& ioservice,
              const cocaine::io::tcp::endpoint& resolver_endpoint);

    template<class Event, typename... Args>
    void
    call(Args&&... args) {
        m_channel->wr->write<Event>(0ul, args...);
    }

    template<class MessageHandler, class ErrorHandler>
    void
    bind(MessageHandler message_handler,
         ErrorHandler error_handler)
    {
        m_channel->rd->bind(message_handler, error_handler);
    }

private:
    void
    connect();

private:
    std::string m_name;
    cocaine::io::reactor_t& m_ioservice;
    std::shared_ptr<resolver_t> m_resolver;
    std::shared_ptr<iochannel_t> m_channel;
};

class service_manager_t :
    private boost::noncopyable
{
    typedef cocaine::io::tcp::endpoint endpoint_t;

public:
    service_manager_t(cocaine::io::reactor_t& ioservice) :
        m_ioservice(ioservice)
    {
        // pass
    }

    template<class ServiceT>
    std::shared_ptr<ServiceT>
    get_service(const endpoint_t& endpoint) {
        return std::shared_ptr<ServiceT>(new ServiceT(endpoint, m_ioservice));
    }

    template<class ServiceT>
    std::shared_ptr<ServiceT>
    get_service(const std::string& name,
                const endpoint_t& resolver = endpoint_t("127.0.0.1", 10053))
    {
        return std::shared_ptr<ServiceT>(new ServiceT(name, m_ioservice, resolver));
    }

private:
    cocaine::io::reactor_t& m_ioservice;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_HPP
