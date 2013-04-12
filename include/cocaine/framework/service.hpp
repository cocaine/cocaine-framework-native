#ifndef COCAINE_FRAMEWORK_SERVICE_HPP
#define COCAINE_FRAMEWORK_SERVICE_HPP

#include <cocaine/framework/resolver.hpp>
#include <cocaine/framework/logging.hpp>

#include <cocaine/forwards.hpp>
#include <cocaine/format.hpp>
#include <cocaine/rpc/channel.hpp>
#include <cocaine/rpc/message.hpp>
#include <cocaine/rpc/protocol.hpp>
#include <cocaine/messages.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/asio/tcp.hpp>
#include <cocaine/asio/reactor.hpp>
#include <cocaine/slot.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <iostream>
#include <functional>
#include <mutex>

namespace cocaine { namespace framework {

class service_error_t :
    public std::runtime_error
{
public:
    explicit service_error_t(const std::string& what) :
        runtime_error(what)
    {
        // pass
    }
};

class logging_service_t;

struct service_handler_concept_t {
    virtual
    ~service_handler_concept_t() {
        // pass
    }

    virtual
    void
    handle_message(const cocaine::io::message_t&) = 0;
};

template<class Event,
         class Result = typename cocaine::io::event_traits<Event>::result_type,
         class = void>
class service_handler :
    public service_handler_concept_t
{
    COCAINE_DECLARE_NONCOPYABLE(service_handler)

    struct empty_functor_t {
        void
        operator()(const Result&)
        {
            // pass
        }
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
    typedef std::function<void(const Result&)>
            message_handler_t;

    typedef std::function<void(cocaine::error_code, const std::string&)>
            error_handler_t;

    struct future {
        future(std::shared_ptr<service_handler> promise) :
            m_promise(promise)
        {
            // pass
        }

        future&
        on_message(message_handler_t handler) {
            m_promise->on_message(handler);
            return *this;
        }

        future&
        on_error(error_handler_t handler) {
            m_promise->on_error(handler);
            return *this;
        }

    private:
        std::shared_ptr<service_handler> m_promise;
    };

    friend class future;

public:
    service_handler() :
        m_message_handler(empty_functor_t()),
        m_error_handler(ignore_error_t())
    {
        // pass
    }

    void
    handle_message(const cocaine::io::message_t&);

private:
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

template<class Event, class Result, class T>
void
service_handler<Event, Result, T>::handle_message(const cocaine::io::message_t& message) {
    if (message.id() == io::event_traits<io::rpc::chunk>::id) {
        std::string data;
        message.as<cocaine::io::rpc::chunk>(data);
        msgpack::unpacked msg;
        msgpack::unpack(&msg, data.data(), data.size());

        m_message_handler(msg.get().as<Result>());
    } else if (message.id() == io::event_traits<io::rpc::error>::id) {
        std::string data;
        message.as<cocaine::io::rpc::chunk>(data);
        msgpack::unpacked msg;
        msgpack::unpack(&msg, data.data(), data.size());

        cocaine::invoke<io::event_traits<io::rpc::error>::tuple_type>::apply(m_error_handler, msg.get());
    }
}

template<class Event, class Result>
class service_handler<
        Event,
        Result,
        typename std::enable_if<
            boost::mpl::is_sequence<Result>::value
        >::type
    > :
    public service_handler_concept_t
{
    COCAINE_DECLARE_NONCOPYABLE(service_handler)

    template<class It, class End, class... Args>
    struct construct_functor {
        typedef typename construct_functor<
                    typename boost::mpl::next<It>::type,
                    End,
                    Args...,
                    typename boost::mpl::deref<It>::type
                >::type
                type;
    };

    template<class End, class... Args>
    struct construct_functor<End, End, Args...> {
        struct type {
            void
            operator()(Args&&... args) {
                // pass
            }
        };
    };

    template<class List>
    struct empty_functor {
        typedef typename construct_functor<
                    typename boost::mpl::begin<List>::type,
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
    typedef typename cocaine::basic_slot<void, Result>::callable_type
            message_handler_t;

    typedef std::function<void(cocaine::error_code, const std::string&)>
            error_handler_t;

    struct future {
        future(std::shared_ptr<service_handler> promise) :
            m_promise(promise)
        {
            // pass
        }

        future&
        on_message(message_handler_t handler) {
            m_promise->on_message(handler);
            return *this;
        }

        future&
        on_error(error_handler_t handler) {
            m_promise->on_error(handler);
            return *this;
        }

    private:
        std::shared_ptr<service_handler> m_promise;
    };

    friend class future;

public:
    service_handler() :
        m_message_handler(typename empty_functor<Result>::type()),
        m_error_handler(ignore_error_t())
    {
        // pass
    }

    void
    handle_message(const cocaine::io::message_t&);

private:
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

template<class Event, class Result>
void
service_handler<
    Event,
    Result,
    typename std::enable_if<
        boost::mpl::is_sequence<Result>::value
    >::type
>
::handle_message(const cocaine::io::message_t& message) {
    if (message.id() == io::event_traits<io::rpc::chunk>::id) {
        std::string data;
        message.as<cocaine::io::rpc::chunk>(data);
        msgpack::unpacked msg;
        msgpack::unpack(&msg, data.data(), data.size());

        cocaine::invoke<Result>::apply(m_message_handler, msg.get());
    } else if (message.id() == io::event_traits<io::rpc::error>::id) {
        std::string data;
        message.as<cocaine::io::rpc::chunk>(data);
        msgpack::unpacked msg;
        msgpack::unpack(&msg, data.data(), data.size());

        cocaine::invoke<
            io::event_traits<io::rpc::error>::tuple_type
        >::apply(m_error_handler, msg.get());
    }
}

template<class Event>
class service_handler<Event, void, void> :
    public service_handler_concept_t
{
    COCAINE_DECLARE_NONCOPYABLE(service_handler)

    struct ignore_error_t {
        void
        operator()(cocaine::error_code,
                   const std::string&)
        {
            // pass
        }
    };

public:
    typedef std::function<void(cocaine::error_code, const std::string&)>
            error_handler_t;

    struct future {
        future(std::shared_ptr<service_handler> promise) :
            m_promise(promise)
        {
            // pass
        }

        future&
        on_error(error_handler_t handler) {
            m_promise->on_error(handler);
            return *this;
        }

    private:
        std::shared_ptr<service_handler> m_promise;
    };

    friend class future;

public:
    service_handler() :
        m_error_handler(ignore_error_t())
    {
        // pass
    }

    void
    handle_message(const cocaine::io::message_t&);

private:
    void
    on_error(error_handler_t handler) {
        m_error_handler = handler;
    }

private:
    error_handler_t m_error_handler;
};

template<class Event>
void
service_handler<Event, void, void>::handle_message(const cocaine::io::message_t& message) {
    if (message.id() == io::event_traits<io::rpc::error>::id) {
        std::string data;
        message.as<cocaine::io::rpc::chunk>(data);
        msgpack::unpacked msg;
        msgpack::unpack(&msg, data.data(), data.size());

        cocaine::invoke<
            io::event_traits<io::rpc::error>::tuple_type
        >::apply(m_error_handler, msg.get());
    }
}

class service_t {
    COCAINE_DECLARE_NONCOPYABLE(service_t)

public:
    template<class Event>
    struct handler {
        typedef service_handler<Event> type;
        typedef typename service_handler<Event>::future future;
    };

public:
    service_t(const std::string& name,
              cocaine::io::reactor_t& ioservice,
              const cocaine::io::tcp::endpoint& resolver_endpoint,
              std::shared_ptr<logging_service_t> logger,
              unsigned int version);

    template<class Event, typename... Args>
    typename handler<Event>::future
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

    int
    version() const {
        return m_version;
    }

    const std::pair<std::string, uint16_t>&
    endpoint() const {
        return m_endpoint;
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
    typedef uint64_t session_id_t;

    std::string m_name;
    unsigned int m_version;
    std::pair<std::string, uint16_t> m_endpoint;

    cocaine::io::reactor_t& m_ioservice;
    std::shared_ptr<resolver_t> m_resolver;
    std::shared_ptr<logging_service_t> m_logger;
    std::shared_ptr<iochannel_t> m_channel;

    session_id_t m_session_counter;
    std::map<session_id_t, std::shared_ptr<service_handler_concept_t>> m_handlers;
    std::mutex m_handlers_lock;
};


template<class Event, typename... Args>
typename service_t::handler<Event>::future
service_t::call(Args&&... args) {
    std::lock_guard<std::mutex> lock(m_handlers_lock);

    auto h = std::make_shared<typename handler<Event>::type>();
    m_handlers[m_session_counter] = h;
    m_channel->wr->write<Event>(m_session_counter, args...);
    ++m_session_counter;
    return typename handler<Event>::future(h);
}

class logging_service_t :
    public std::enable_shared_from_this<logging_service_t>,
    public service_t
{
public:
    logging_service_t(const std::string& name,
                      cocaine::io::reactor_t& service,
                      const cocaine::io::tcp::endpoint& resolver,
                      std::shared_ptr<logging_service_t> logger, // OMG
                      const std::string& source) :
        service_t(name,
                  service,
                  resolver,
                  logger,
                  cocaine::io::protocol<cocaine::io::logging_tag>::version::value),
        m_priority(cocaine::logging::priorities::warning),
        m_source(source)
    {
        // pass
    }

    void
    emit(cocaine::logging::priorities priority,
         const std::string& message)
    {
        call<cocaine::io::logging::emit>(priority, m_source, message);
    }

    template<typename... Args>
    void
    emit(cocaine::logging::priorities priority,
         const std::string& format,
         const Args&... args)
    {
        emit(priority, cocaine::format(format, args...));
    }

    cocaine::logging::priorities
    verbosity() const {
        return m_priority;
    }

    void
    initialize();

protected:
    void
    on_verbosity_response(cocaine::io::reactor_t *ioservice,
                          const cocaine::io::message_t& message);

private:
    cocaine::logging::priorities m_priority;
    std::string m_source;
};

class service_manager_t {
    COCAINE_DECLARE_NONCOPYABLE(service_manager_t)

    typedef cocaine::io::tcp::endpoint endpoint_t;

public:
    service_manager_t(cocaine::io::reactor_t& ioservice,
                      const std::string& logging_prefix) :
        m_ioservice(ioservice)
    {
        m_logger = std::make_shared<logging_service_t>(
                       "logging",
                       m_ioservice,
                       endpoint_t("127.0.0.1", 10053),
                       std::shared_ptr<logging_service_t>(),
                       logging_prefix
                   );
    }

    template<class Service, typename... Args>
    std::shared_ptr<Service>
    get_service(const std::string& name,
                Args&&... args)
    {
        auto new_service = std::make_shared<Service>(name,
                                                     m_ioservice,
                                                     endpoint_t("127.0.0.1", 10053),
                                                     m_logger,
                                                     std::forward<Args>(args)...);
        new_service->initialize();
        return new_service;
    }

    std::shared_ptr<logging_service_t>
    get_system_logger() {
        return m_logger;
    }

private:
    cocaine::io::reactor_t& m_ioservice;

    std::shared_ptr<logging_service_t> m_logger;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_HPP
