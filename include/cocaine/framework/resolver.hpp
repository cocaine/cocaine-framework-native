#ifndef COCAINE_FRAMEWORK_RESOLVER_HPP
#define COCAINE_FRAMEWORK_RESOLVER_HPP

#include <cocaine/rpc/channel.hpp>
#include <cocaine/rpc/message.hpp>
#include <cocaine/rpc/protocol.hpp>
#include <cocaine/messages.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/asio/tcp.hpp>
#include <cocaine/asio/reactor.hpp>

#include <string>
#include <stdexcept>

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
    typedef cocaine::tuple::fold<
        cocaine::io::locator::resolve::result_type
    >::type result_type;

    struct error_t {
        int code;
        std::string message;
    };

public:
    resolver_t(const cocaine::io::tcp::endpoint& endpoint);

    const result_type&
    resolve(const std::string& service_name);

private:
    void
    on_response(const cocaine::io::message_t& message);

    void
    on_error(const std::error_code&);

private:
    cocaine::io::reactor_t m_ioservice;
    cocaine::io::channel<
        cocaine::io::socket<cocaine::io::tcp>
    > m_channel;

    result_type m_last_response;

    bool m_error_flag;
    error_t m_last_error;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_RESOLVER_HPP
