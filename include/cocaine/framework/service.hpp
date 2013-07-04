#ifndef COCAINE_FRAMEWORK_SERVICE_HPP
#define COCAINE_FRAMEWORK_SERVICE_HPP

#include <cocaine/framework/stream.hpp>
#include <cocaine/framework/logging.hpp>
#include <cocaine/framework/service_error.hpp>

#include <cocaine/rpc/channel.hpp>
#include <cocaine/rpc/message.hpp>
#include <cocaine/rpc/protocol.hpp>
#include <cocaine/traits/typelist.hpp>
#include <cocaine/traits/tuple.hpp>
#include <cocaine/messages.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/asio/tcp.hpp>
#include <cocaine/asio/reactor.hpp>

#include <memory>
#include <string>
#include <functional>
#include <utility>
#include <mutex>
#include <thread>

namespace cocaine { namespace framework {

class service_t;
class service_manager_t;

namespace detail { namespace service {

    struct service_handler_concept_t {
        virtual
        ~service_handler_concept_t() {
            // pass
        }

        virtual
        void
        handle_message(const cocaine::io::message_t&) = 0;

        virtual
        void
        error(std::exception_ptr e) = 0;
    };

    template<template<class...> class Wrapper, class... Args>
    struct wrapper_traits {
        typedef Wrapper<Args...> type;
    };

    template<template<class...> class Wrapper, class... Args>
    struct wrapper_traits<Wrapper, std::tuple<Args...>> {
        typedef Wrapper<Args...> type;
    };

    template<class Event, class ResultType = typename cocaine::io::event_traits<Event>::result_type>
    struct unpacker {
        typedef typename wrapper_traits<cocaine::framework::stream, ResultType>::type promise_type;
        typedef typename wrapper_traits<cocaine::framework::generator, ResultType>::type future_type;

        static
        inline
        void
        unpack(promise_type& p,
               std::string& data)
        {
            msgpack::unpacked msg;
            msgpack::unpack(&msg, data.data(), data.size());

            ResultType r;
            cocaine::io::type_traits<ResultType>::unpack(msg.get(), r);
            p.push_value(std::move(r));
        }
    };

    template<class Event>
    struct unpacker<Event, cocaine::io::raw_t> {
        typedef typename wrapper_traits<cocaine::framework::stream, std::string>::type promise_type;
        typedef typename wrapper_traits<cocaine::framework::generator, std::string>::type future_type;

        static
        inline
        void
        unpack(promise_type& p,
               std::string& data)
        {
            p.push_value(std::move(data));
        }
    };

    template<class Event, class ResultType = typename cocaine::io::event_traits<Event>::result_type>
    struct message_handler {
        static
        inline
        void
        handle(typename unpacker<Event>::promise_type& p,
               const cocaine::io::message_t& message)
        {
            if (message.id() == io::event_traits<io::rpc::chunk>::id) {
                std::string data;
                message.as<cocaine::io::rpc::chunk>(data);
                unpacker<Event>::unpack(p, data);
            } else if (message.id() == io::event_traits<io::rpc::error>::id) {
                int code;
                std::string msg;
                message.as<cocaine::io::rpc::error>(code, msg);
                p.set_exception(
                    service_error_t(std::error_code(code, service_response_category()), msg)
                );
            }
        }
    };

    template<class Event>
    struct message_handler<Event, void> {
        static
        inline
        void
        handle(typename unpacker<Event>::promise_type& p,
               const cocaine::io::message_t& message)
        {
            if (message.id() == io::event_traits<io::rpc::choke>::id) {
                p.close();
            } else if (message.id() == io::event_traits<io::rpc::error>::id) {
                int code;
                std::string msg;
                message.as<cocaine::io::rpc::error>(code, msg);
                p.set_exception(
                    service_error_t(std::error_code(code, service_response_category()), msg)
                );
            }
        }
    };

    template<class Event>
    class service_handler :
        public service_handler_concept_t
    {
        COCAINE_DECLARE_NONCOPYABLE(service_handler)

    public:
        typedef typename unpacker<Event>::future_type
                future_type;

        typedef typename unpacker<Event>::promise_type
                promise_type;

        service_handler()
        {
            // pass
        }

        service_handler(service_handler&& other) :
            m_promise(std::move(other.m_promise))
        {
            // pass
        }

        future_type
        get_future() {
            return m_promise.get_generator();
        }

        void
        handle_message(const cocaine::io::message_t& message) {
            message_handler<Event>::handle(m_promise, message);
        }

        void
        error(std::exception_ptr e) {
            m_promise.set_exception(e);
        }

    protected:
        promise_type m_promise;
    };

}} // detail::service

template<class Event>
struct service_traits {
    typedef typename detail::service::service_handler<Event>::future_type
            future_type;
};

class service_connection_t :
    public std::enable_shared_from_this<service_connection_t>
{
    COCAINE_DECLARE_NONCOPYABLE(service_connection_t)

    friend class service_manager_t;
    friend class service_t;

public:
    enum class status_t {
        disconnected,
        connecting,
        waiting_for_sessions,
        connected
    };

    ~service_connection_t();

    std::string
    name() const {
        if (m_name) {
            return *m_name;
        } else {
            return cocaine::format("%s:%d", m_endpoint.address(), m_endpoint.port());
        }
    }

    int
    version() const {
        return m_version;
    }

    status_t
    status() const {
        return m_connection_status;
    }

    void
    throw_when_reconnecting(bool t) {
        m_throw_when_reconnecting = t;
    }

    template<class Event, typename... Args>
    typename service_traits<Event>::future_type
    call(Args&&... args);

    future<std::shared_ptr<service_connection_t>>
    reconnect();

    void
    soft_destroy();

private:
    typedef cocaine::io::tcp::endpoint
            endpoint_t;

private:
    service_connection_t(const std::string& name,
                         std::shared_ptr<service_manager_t> manager,
                         unsigned int version);

    service_connection_t(const endpoint_t& endpoint,
                         std::shared_ptr<service_manager_t> manager,
                         unsigned int version);

    std::shared_ptr<service_manager_t>
    manager() {
        auto m = m_manager.lock();
        if (m) {
            return m;
        } else {
            throw service_error_t(service_errc::broken_manager);
        }
    }

    void
    use_default_executor(bool use) {
        m_use_default_executor = use;
    }

    future<std::shared_ptr<service_connection_t>>
    connect(std::unique_lock<std::recursive_mutex>&);

    void
    connect_to_endpoint();

    void
    reset_sessions();

    std::shared_ptr<service_connection_t>
    on_resolved(service_traits<cocaine::io::locator::resolve>::future_type&);

    void
    on_message(const cocaine::io::message_t& message);

    void
    on_error(const std::error_code&);

private:
    typedef cocaine::io::channel<cocaine::io::socket<cocaine::io::tcp>>
            iochannel_t;

    typedef uint64_t session_id_t;

    typedef std::map<session_id_t, std::shared_ptr<detail::service::service_handler_concept_t>>
            handlers_map_t;

private:
    boost::optional<std::string> m_name;
    endpoint_t m_endpoint;
    unsigned int m_version;

    std::weak_ptr<service_manager_t> m_manager;
    std::unique_ptr<iochannel_t> m_channel;
    bool m_throw_when_reconnecting;
    status_t m_connection_status;

    // resolver must not use default executor, that posts handlers to main event loop
    bool m_use_default_executor;

    std::atomic<session_id_t> m_session_counter;
    handlers_map_t m_handlers;
    std::recursive_mutex m_handlers_lock;
};

class service_manager_t :
    public std::enable_shared_from_this<service_manager_t>
{
    COCAINE_DECLARE_NONCOPYABLE(service_manager_t)

    friend class service_connection_t;
    friend class service_t;

    typedef cocaine::io::tcp::endpoint endpoint_t;

public:
    static
    std::shared_ptr<service_manager_t>
    create(endpoint_t resolver_endpoint,
           const std::string& logging_prefix,
           const executor_t& executor = executor_t())
    {
        std::shared_ptr<service_manager_t> manager(
            new service_manager_t(resolver_endpoint, executor)
        );
        manager->init();
        manager->init_logger(logging_prefix);
        return manager;
    }

    static
    std::shared_ptr<service_manager_t>
    create(endpoint_t resolver_endpoint,
           std::shared_ptr<logger_t> logger = std::shared_ptr<logger_t>(),
           const executor_t& executor = executor_t())
    {
        std::shared_ptr<service_manager_t> manager(
            new service_manager_t(resolver_endpoint, executor)
        );
        manager->init();
        manager->m_logger = logger;
        return manager;
    }

    ~service_manager_t();

    template<class Service, typename... Args>
    std::shared_ptr<Service>
    get_service(const std::string& name,
                Args&&... args)
    {
        std::shared_ptr<service_connection_t> service(
            new service_connection_t(name, shared_from_this(), Service::version)
        );
        std::unique_lock<std::recursive_mutex> lock(service->m_handlers_lock);
        service->connect(lock).get();
        return std::make_shared<Service>(service, std::forward<Args>(args)...);
    }

    template<class Service, typename... Args>
    future<std::shared_ptr<Service>>
    get_service_async(const std::string& name,
                      Args&&... args)
    {
        std::shared_ptr<service_connection_t> service(
            new service_connection_t(name, shared_from_this(), Service::version)
        );
        std::unique_lock<std::recursive_mutex> lock(service->m_handlers_lock);
        auto f = service->connect(lock)
            .then(executor_t(),
                  std::bind(&service_manager_t::make_service_stub,
                            std::placeholders::_1,
                            std::forward<Args>(args)...));
        f.set_default_executor(m_default_executor);
        return f;
    }

    std::shared_ptr<logger_t>&
    get_system_logger() {
        return m_logger;
    }

private:
    service_manager_t(endpoint_t resolver_endpoint,
                      const executor_t& executor) :
        m_resolver_endpoint(resolver_endpoint),
        m_default_executor(executor)
    {
        // pass
    }

    void
    init();

    void
    init_logger(const std::string& logging_prefix);

    service_traits<cocaine::io::locator::resolve>::future_type
    resolve(const std::string& name) {
        return m_resolver->call<cocaine::io::locator::resolve>(name);
    }

    void
    register_connection(std::shared_ptr<service_connection_t> connection);

    void
    release_connection(std::shared_ptr<service_connection_t> connection);

    void
    remove_connection(std::shared_ptr<service_connection_t> connection);

    template<class Service, class... Args>
    static
    std::shared_ptr<Service>
    make_service_stub(future<std::shared_ptr<service_connection_t>>& f, Args&&... args) {
        return std::make_shared<Service>(f.get(), std::forward<Args>(args)...);
    }

private:
    cocaine::io::reactor_t m_ioservice;
    std::thread m_working_thread;
    endpoint_t m_resolver_endpoint;
    executor_t m_default_executor;

    std::set<std::shared_ptr<service_connection_t>> m_connections;
    std::mutex m_connections_lock;

    std::shared_ptr<service_connection_t> m_resolver;
    std::shared_ptr<logger_t> m_logger;
};

template<class Event, typename... Args>
typename service_traits<Event>::future_type
service_connection_t::call(Args&&... args) {
    std::unique_lock<std::recursive_mutex> lock(m_handlers_lock);
    typename service_traits<Event>::future_type f;
    if (m_connection_status == status_t::disconnected) {
        throw service_error_t(service_errc::not_connected);
    } else if (m_connection_status == status_t::connecting && m_throw_when_reconnecting) {
        throw service_error_t(service_errc::wait_for_connection);
    } else if (m_connection_status == status_t::waiting_for_sessions) {
        throw service_error_t(service_errc::wait_for_connection);
    } else {
        // prepare future
        auto h = std::make_shared<detail::service::service_handler<Event>>();
        f = h->get_future();
        if (m_use_default_executor) {
            f.set_default_executor(manager()->m_default_executor);
        }

        // create session and do call
        session_id_t current_session = m_session_counter++;
        m_handlers[current_session] = h;
        m_channel->wr->write<Event>(current_session, std::forward<Args>(args)...);
    }
    return f;
}

struct service_t {
    COCAINE_DECLARE_NONCOPYABLE(service_t)

    service_t(std::shared_ptr<service_connection_t> connection) :
        m_connection(connection)
    {
        auto m = connection->m_manager.lock();
        if (m) {
            m->register_connection(connection);
        }
    }

    virtual
    ~service_t() {
        auto c = m_connection.lock();
        if (c) {
            auto m = c->m_manager.lock();
            if (m) {
                m->release_connection(c);
            }
        }
    }

    std::string
    name() {
        return connection()->name();
    }

    service_connection_t::status_t
    status() {
        return connection()->status();
    }

    template<class Event, typename... Args>
    typename service_traits<Event>::future_type
    call(Args&&... args) {
        return connection()->call<Event>(std::forward<Args>(args)...);
    }

    void
    reconnect() {
        connection()->reconnect().get();
    }

    future<void>
    async_reconnect() {
        return connection()->reconnect().then(&service_t::empty);
    }

    void
    soft_destroy() {
        connection()->soft_destroy();
        m_connection.reset();
    }

private:
    std::shared_ptr<service_connection_t>
    connection() {
        auto c = m_connection.lock();
        if (c) {
            return c;
        } else {
            throw service_error_t(service_errc::broken_manager);
        }
    }

    static
    void
    empty(future<std::shared_ptr<service_connection_t>>& f) {
        f.get();
    }

private:
    std::weak_ptr<service_connection_t> m_connection;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_HPP
