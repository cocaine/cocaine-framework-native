#ifndef COCAINE_FRAMEWORK_SERVICE_HPP
#define COCAINE_FRAMEWORK_SERVICE_HPP

#include <cocaine/rpc/channel.hpp>
#include <cocaine/rpc/message.hpp>
#include <cocaine/messages.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/asio/tcp.hpp>
#include <cocaine/asio/reactor.hpp>
#include <cocaine/slot.hpp>

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
    typedef tuple::fold<
        io::locator::resolve::result_type
    >::type result_type;

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
    struct error_t {
        cocaine::error_code code;
        std::string message;
    };

private:
    cocaine::io::reactor_t m_ioservice;
    cocaine::io::channel<
        cocaine::io::socket<cocaine::io::tcp>
    > m_channel;

    result_type m_last_response;

    bool m_error_flag;
    error_t m_last_error;
};

struct basic_service_handler {
    virtual
    void
    handle_message(const cocaine::io::message_t&) = 0;
};

template<class Event>
class service_result_handler :
    private boost::noncopyable,
    public basic_service_handler
{
    template<class It, class End, class... Args>
    struct construct_functor {
        typedef typename construct_functor<typename boost::mpl::next<It>::type,
                                          End,
                                          Args...,
                                          typename boost::mpl::deref<It>::type
                                          >::type
                type;
    };

    template<class End, class... Args>
    struct construct_functor<End, End, Args...> {
        struct functor {
            void
            operator()(Args&&... args) {
                // pass
            }
        };

        typedef functor type;
    };

    template<class List>
    struct empty_functor {
        typedef typename construct_functor<typename boost::mpl::begin<List>::type,
                                           typename boost::mpl::end<List>::type
                                           >::type
                type;
    };

    struct ignore_error_t {
        void
        operator()(cocaine::error_code,
                   const std::string&)
        {
            // pass
        }
    };

public:
    typedef typename cocaine::basic_slot<void, typename Event::result_type>::callable_type
            message_handler_t;

    typedef std::function<void(cocaine::error_code, const std::string&)>
            error_handler_t;

public:
    service_result_handler() :
        m_message_handler(typename empty_functor<typename Event::result_type>::type()),
        m_error_handler(ignore_error_t())
    {
        // pass
    }

    void
    handle_message(const cocaine::io::message_t&);

    void
    on_message(message_handler_t handler) {
        m_message_handler = handler;
    }

    void
    on_error(error_handler_t handler) {
        m_error_handler = handler;
    }

private:
    message_handler_t m_message_handler;
    error_handler_t m_error_handler;
};

template<class Event>
void
service_result_handler<Event>::handle_message(const cocaine::io::message_t& message) {
    if (message.id() == io::event_traits<io::rpc::chunk>::id) {
        std::string data;
        message.as<cocaine::io::rpc::chunk>(data);
        msgpack::unpacked msg;
        msgpack::unpack(&msg, data.data(), data.size());

        cocaine::blocking_slot<void, typename Event::result_type> slot("", m_message_handler);
        slot(std::shared_ptr<cocaine::api::stream_t>(), msg.get());
    } else if (message.id() == io::event_traits<io::rpc::error>::id) {
        std::string data;
        message.as<cocaine::io::rpc::chunk>(data);
        msgpack::unpacked msg;
        msgpack::unpack(&msg, data.data(), data.size());

        cocaine::blocking_slot<void, cocaine::io::rpc::error::tuple_type> slot("", m_error_handler);
        slot(std::shared_ptr<cocaine::api::stream_t>(), msg.get());
    }
}

class service_t :
    private boost::noncopyable
{
public:
    typedef uint64_t session_id_t;

public:
    service_t(const std::string& name,
              cocaine::io::reactor_t& ioservice,
              const cocaine::io::tcp::endpoint& resolver_endpoint);

    template<class Event, typename... Args>
    std::shared_ptr<service_result_handler<Event>>
    call(Args&&... args);

    virtual
    void
    initialize() {
        // pass
    }

    const std::string&
    name() const {
        return m_name;
    }

    std::shared_ptr<resolver_t>
    resolver() const {
        return m_resolver;
    }

protected:
    void
    on_error(const std::error_code&);

private:
    typedef cocaine::io::channel<cocaine::io::socket<cocaine::io::tcp>>
            iochannel_t;

private:
    void
    connect();

    void
    on_message(const cocaine::io::message_t& message);

private:
    std::string m_name;
    cocaine::io::reactor_t& m_ioservice;
    std::shared_ptr<resolver_t> m_resolver;
    std::shared_ptr<iochannel_t> m_channel;

    session_id_t m_session_counter;
    std::map<session_id_t, std::shared_ptr<basic_service_handler>> m_handlers;
};


template<class Event, typename... Args>
std::shared_ptr<service_result_handler<Event>>
service_t::call(Args&&... args) {
    auto handler = std::make_shared<service_result_handler<Event>>();
    m_handlers[m_session_counter] = handler;
    m_channel->wr->write<Event>(m_session_counter, args...);
    ++m_session_counter;
    return handler;
}

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

    template<class Service>
    std::shared_ptr<Service>
    get_service(const std::string& name,
                const endpoint_t& resolver = endpoint_t("127.0.0.1", 10053))
    {
        auto new_service = std::make_shared<Service>(name, m_ioservice, resolver);
        new_service->initialize();
        return new_service;
    }

private:
    cocaine::io::reactor_t& m_ioservice;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_HPP
