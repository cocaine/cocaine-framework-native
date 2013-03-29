#ifndef COCAINE_FRAMEWORK_SERVICE_HPP
#define COCAINE_FRAMEWORK_SERVICE_HPP

#include <cocaine/detail/locator.hpp>
#include <cocaine/rpc/channel.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/asio/tcp.hpp>
#include <cocaine/asio/reactor.hpp>

#include <memory>
#include <string>
#include <functional>
#include <stdexcept>
#include <boost/utility.hpp>

namespace cocaine { namespace framework {

struct ignore_message_t {
    void operator()(const cocaine::io::message_t&) {
        // pass
    }
};

extern const ignore_message_t ignore_message;

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
    typedef std::function<void(const cocaine::io::message_t&)>
            message_handler_t;

public:
    service_t(const std::string& name,
              cocaine::io::reactor_t& ioservice,
              const cocaine::io::tcp::endpoint& resolver_endpoint);

    template<class Event, typename... Args>
    void
    call(const message_handler_t& handler, Args&&... args) {
        m_handlers[m_session_counter] = handler;
        m_channel->wr->write<Event>(m_session_counter, args...);
        ++m_session_counter;
    }

private:
    void
    connect();

    void
    on_message(const cocaine::io::message_t& message);

    void
    on_error(const std::error_code&);

private:
    std::string m_name;
    cocaine::io::reactor_t& m_ioservice;
    std::shared_ptr<resolver_t> m_resolver;
    std::shared_ptr<iochannel_t> m_channel;

    uint64_t m_session_counter;
    std::map<unsigned int, message_handler_t> m_handlers;
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
