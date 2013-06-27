#ifndef COCAINE_FRAMEWORK_SERVICE_HPP
#define COCAINE_FRAMEWORK_SERVICE_HPP

#include <cocaine/framework/future.hpp>
#include <cocaine/framework/stream.hpp>
#include <cocaine/framework/logging.hpp>
#include <cocaine/framework/service_error.hpp>

#include <cocaine/rpc/channel.hpp>
#include <cocaine/rpc/message.hpp>
#include <cocaine/rpc/protocol.hpp>
#include <cocaine/traits/enum.hpp>
#include <cocaine/traits/literal.hpp>
#include <cocaine/traits/typelist.hpp>
#include <cocaine/traits/tuple.hpp>
#include <cocaine/traits/literal.hpp>
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

    template<class T, bool streamed>
    struct future_traits {
        typedef typename wrapper_traits<cocaine::framework::promise, T>::type promise_type;
        typedef typename wrapper_traits<cocaine::framework::future, T>::type future_type;

        static
        inline
        future_type
        get_future(promise_type& p) {
            return p.get_future();
        }

        template<class... Args>
        static
        inline
        void
        push_value(promise_type& p,
                   Args&&... args)
        {
             p.set_value(std::forward<Args>(args)...);
        }
    };

    template<class T>
    struct future_traits<T, true> {
        typedef typename wrapper_traits<cocaine::framework::stream, T>::type promise_type;
        typedef typename wrapper_traits<cocaine::framework::generator, T>::type future_type;

        static
        inline
        future_type
        get_future(promise_type& p) {
            return p.get_generator();
        }

        template<class... Args>
        static
        inline
        void
        push_value(promise_type& p,
                   Args&&... args)
        {
             p.push_value(std::forward<Args>(args)...);
        }
    };

    template<class Event, class ResultType = typename cocaine::io::event_traits<Event>::result_type>
    struct unpacker {
        typedef future_traits<ResultType, cocaine::io::event_traits<Event>::streamed>
                traits;

        static
        inline
        void
        unpack(typename traits::promise_type& p,
               std::string& data)
        {
            msgpack::unpacked msg;
            msgpack::unpack(&msg, data.data(), data.size());

            ResultType r;
            cocaine::io::type_traits<ResultType>::unpack(msg.get(), r);
            traits::push_value(p, std::move(r));
        }
    };

    template<class Event>
    struct unpacker<Event, cocaine::io::raw_t> {
        typedef future_traits<std::string, cocaine::io::event_traits<Event>::streamed>
        traits;

        static
        inline
        void
        unpack(typename traits::promise_type& p,
               std::string& data)
        {
            traits::push_value(p, std::move(data));
        }
    };

    template<class Event, class ResultType = typename cocaine::io::event_traits<Event>::result_type>
    struct message_handler {
        typedef typename unpacker<Event>::traits
                traits;

        static
        inline
        void
        handle(typename traits::promise_type& p,
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
        typedef typename unpacker<Event>::traits
                traits;

        static
        inline
        void
        handle(typename traits::promise_type& p,
               const cocaine::io::message_t& message)
        {
            if (message.id() == io::event_traits<io::rpc::choke>::id) {
                p.set_value();
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
        typedef typename unpacker<Event>::traits::future_type
                future_type;

        typedef typename unpacker<Event>::traits::promise_type
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
            return unpacker<Event>::traits::get_future(m_promise);
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

class service_t :
    public std::enable_shared_from_this<service_t>
{
    COCAINE_DECLARE_NONCOPYABLE(service_t)

    friend class service_manager_t;

    typedef cocaine::io::channel<cocaine::io::socket<cocaine::io::tcp>>
            iochannel_t;

    typedef cocaine::io::tcp::endpoint
            endpoint_t;

    typedef uint64_t session_id_t;

    typedef std::map<session_id_t, std::shared_ptr<detail::service::service_handler_concept_t>>
            handlers_map_t;

public:
    template<class Event>
    struct handler {
        typedef detail::service::service_handler<Event>
                type;

        typedef typename detail::service::service_handler<Event>::future_type
                future;

        typedef typename detail::service::service_handler<Event>::promise_type
                promise;
    };

    enum class status {
        disconnected,
        connecting,
        connected
    };

public:
    void
    reconnect();

    future<std::shared_ptr<service_t>>
    reconnect_async();

    void
    throw_when_reconnecting(bool t) {
        m_throw_when_reconnecting = t;
    }

    template<class Event, typename... Args>
    typename handler<Event>::future
    call(Args&&... args);

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

private:
    service_t(const std::string& name,
              std::shared_ptr<service_manager_t> manager,
              unsigned int version);

    service_t(const endpoint_t& endpoint,
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
    on_message(const cocaine::io::message_t& message);

    void
    on_error(const std::error_code&);

    std::shared_ptr<service_t>
    on_resolved(service_t::handler<cocaine::io::locator::resolve>::future&);

    void
    connect_to_endpoint();

    void
    reset_sessions();

private:
    boost::optional<std::string> m_name;
    endpoint_t m_endpoint;
    unsigned int m_version;

    std::weak_ptr<service_manager_t> m_manager;
    std::unique_ptr<iochannel_t> m_channel;
    bool m_throw_when_reconnecting;
    status m_connection_status;

    session_id_t m_session_counter;
    handlers_map_t m_handlers;
    std::mutex m_handlers_lock;
};

class service_manager_t :
    public std::enable_shared_from_this<service_manager_t>
{
    COCAINE_DECLARE_NONCOPYABLE(service_manager_t)

    friend class service_t;

    typedef cocaine::io::tcp::endpoint endpoint_t;

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

public:
    static
    std::shared_ptr<service_manager_t>
    create(endpoint_t resolver_endpoint,
           const std::string& logging_prefix,
           const executor_t& executor = executor_t())
    {
        auto manager = std::shared_ptr<service_manager_t>(
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
        auto manager = std::shared_ptr<service_manager_t>(
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
        std::shared_ptr<service_t> service(new service_t(name, shared_from_this(), Service::version));
        service->reconnect();
        return std::make_shared<Service>(service, std::forward<Args>(args)...);
    }

    std::shared_ptr<service_t>
    get_service(const std::string& name,
                unsigned int version)
    {
        std::shared_ptr<service_t> service(new service_t(name, shared_from_this(), version));
        service->reconnect();
        return service;
    }

    template<class Service, typename... Args>
    future<std::shared_ptr<Service>>
    get_service_async(const std::string& name,
                      Args&&... args)
    {
        std::shared_ptr<service_t> service(new service_t(name, shared_from_this(), Service::version));
        auto f = service->reconnect_async()
            .then(executor_t(),
                  std::bind(&service_manager_t::make_service_stub,
                            std::placeholders::_1,
                            std::forward<Args>(args)...));
        f.set_default_executor(m_default_executor);
        return f;
    }

    future<std::shared_ptr<service_t>>
    get_service_async(const std::string& name,
                      unsigned int version)
    {
        std::shared_ptr<service_t> service(new service_t(name, shared_from_this(), version));
        return service->reconnect_async();
    }

    std::shared_ptr<logger_t>&
    get_system_logger() {
        return m_logger;
    }

    service_t::handler<cocaine::io::locator::resolve>::future
    async_resolve(const std::string& name) {
        return m_resolver->call<cocaine::io::locator::resolve>(name);
    }

    const std::shared_ptr<service_t>&
    get_resolver() const {
        return m_resolver;
    }

private:
    template<class Service, class... Args>
    static
    std::shared_ptr<Service>
    make_service_stub(future<std::shared_ptr<service_t>>& f, Args&&... args) {
        return std::make_shared<Service>(f.get(), std::forward<Args>(args)...);
    }

private:
    cocaine::io::reactor_t m_ioservice;
    std::thread m_working_thread;
    endpoint_t m_resolver_endpoint;
    executor_t m_default_executor;
    std::shared_ptr<service_t> m_resolver;
    std::shared_ptr<logger_t> m_logger;
};

template<class Event, typename... Args>
typename service_t::handler<Event>::future
service_t::call(Args&&... args) {
    if (m_connection_status == status::disconnected) {
        throw service_error_t(service_errc::not_connected);
    } else if (m_connection_status == status::connecting && m_throw_when_reconnecting) {
        throw service_error_t(service_errc::wait_for_connection);
    } else {
        std::lock_guard<std::mutex> lock(m_handlers_lock);

        auto h = std::make_shared<typename service_t::handler<Event>::type>();
        auto f = h->get_future();
        f.set_default_executor(manager()->m_default_executor);
        m_channel->wr->write<Event>(m_session_counter, std::forward<Args>(args)...);
        m_handlers[m_session_counter] = h;
        ++m_session_counter;
        return f;
    }
}

struct service_stub_t {
    service_stub_t(std::shared_ptr<service_t> service) :
        m_service(service)
    {
        // pass
    }

    virtual
    ~service_stub_t() {
        // pass
    }

    std::shared_ptr<service_t>
    backend() const {
        return m_service;
    }

    void
    reconnect() {
        m_service->reconnect();
    }

    future<void>
    reconnect_async() {
        return m_service->reconnect_async().then(&service_stub_t::empty);
    }

private:
    static
    void
    empty(future<std::shared_ptr<service_t>>&) {
        // pass
    }

private:
    std::shared_ptr<service_t> m_service;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_HPP
