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
#include <cocaine/traits/typelist.hpp>
#include <cocaine/traits/tuple.hpp>
#include <cocaine/traits/string_literal.hpp>
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
#include <iostream>

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
    struct wrapper_trait {
        typedef Wrapper<Args...> type;
    };

    template<template<class...> class Wrapper, class... Args>
    struct wrapper_trait<Wrapper, std::tuple<Args...>> {
        typedef Wrapper<Args...> type;
    };

    template<class T>
    struct future_trait {
        typedef typename wrapper_trait<cocaine::framework::promise, T>::type promise_type;
        typedef typename wrapper_trait<cocaine::framework::future, T>::type future_type;
        typedef T result_type;

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
        push_value(promise_type& p, Args&&... args) {
            return p.set_value(std::forward<Args>(args)...);
        }
    };

    template<class T>
    struct future_trait<cocaine::framework::stream<T>> {
        typedef typename wrapper_trait<cocaine::framework::stream, T>::type promise_type;
        typedef typename wrapper_trait<cocaine::framework::generator, T>::type future_type;
        typedef T result_type;

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
        push_value(promise_type& p, Args&&... args) {
            return p.push_value(std::forward<Args>(args)...);
        }
    };

    template<class ResultType>
    inline
    void
    set_promise_result(typename future_trait<ResultType>::promise_type& p,
                       const cocaine::io::message_t& message)
    {
        if (message.id() == io::event_traits<io::rpc::chunk>::id) {
            std::string data;
            message.as<cocaine::io::rpc::chunk>(data);
            msgpack::unpacked msg;
            msgpack::unpack(&msg, data.data(), data.size());

            typename future_trait<ResultType>::result_type r;
            cocaine::io::type_traits<typename future_trait<ResultType>::result_type>::unpack(msg.get(), r);
            future_trait<ResultType>::push_value(p, std::move(r));

            std::cout << "set_promise chunk" << std::endl;
        } else if (message.id() == io::event_traits<io::rpc::error>::id) {
            int code;
            std::string msg;
            message.as<cocaine::io::rpc::error>(code, msg);
            p.set_exception(
                service_error_t(std::error_code(code, service_response_category()), msg)
            );
            std::cout << "set_promise error" << std::endl;
        } else if (message.id() == io::event_traits<io::rpc::choke>::id) {
            std::cout << "set_promise choke" << std::endl;
        } else {
            std::cout << "set_promise xz" << std::endl;
        }
    }

    template<>
    inline
    void
    set_promise_result<void>(promise<void>& p,
                             const cocaine::io::message_t& message)
    {
        if (message.id() == io::event_traits<io::rpc::choke>::id) {
            p.set_value();

            std::cout << "set_promise choke" << std::endl;
        } else if (message.id() == io::event_traits<io::rpc::error>::id) {
            int code;
            std::string msg;
            message.as<cocaine::io::rpc::error>(code, msg);
            p.set_exception(
                service_error_t(std::error_code(code, service_response_category()), msg)
            );
            std::cout << "set_promise error" << std::endl;
        } else if (message.id() == io::event_traits<io::rpc::chunk>::id) {
            std::cout << "set_promise chunk" << std::endl;
        } else {
            std::cout << "set_promise xz" << std::endl;
        }
    }

    template<class Event>
    class service_handler :
        public service_handler_concept_t
    {
        COCAINE_DECLARE_NONCOPYABLE(service_handler)

    public:
        typedef typename cocaine::io::event_traits<Event>::result_type
                result_type;

        typedef typename detail::service::future_trait<result_type>::future_type
                future_type;

        typedef typename detail::service::future_trait<result_type>::promise_type
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
            return detail::service::future_trait<result_type>::get_future(m_promise);
        }

        void
        handle_message(const cocaine::io::message_t& message) {
            set_promise_result<result_type>(m_promise, message);
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

public:
    void
    reconnect();

    future<std::shared_ptr<service_t>>
    reconnect_async();

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
              service_manager_t& manager,
              unsigned int version);

    service_t(const endpoint_t& endpoint,
              service_manager_t& manager,
              unsigned int version);

    void
    on_message(const cocaine::io::message_t& message);

    void
    on_error(const std::error_code&);

    std::shared_ptr<service_t>
    on_resolved(service_t::handler<cocaine::io::locator::resolve>::future&);

    void
    connect_to_endpoint();

private:
    boost::optional<std::string> m_name;
    endpoint_t m_endpoint;
    unsigned int m_version;

    service_manager_t& m_manager;
    std::unique_ptr<iochannel_t> m_channel;

    session_id_t m_session_counter;
    handlers_map_t m_handlers;
    std::mutex m_handlers_lock;
};

class service_manager_t {
    COCAINE_DECLARE_NONCOPYABLE(service_manager_t)

    friend class service_t;

    typedef cocaine::io::tcp::endpoint endpoint_t;

public:
    service_manager_t(endpoint_t resolver_endpoint,
                      const std::string& logging_prefix,
                      const executor_t& executor);

    ~service_manager_t();

    template<class Service, typename... Args>
    std::shared_ptr<Service>
    get_service(const std::string& name,
                Args&&... args)
    {
        std::shared_ptr<service_t> service(new service_t(name, *this, Service::version));
        service->reconnect();
        return std::make_shared<Service>(service, std::forward<Args>(args)...);
    }

    std::shared_ptr<service_t>
    get_service(const std::string& name,
                unsigned int version)
    {
        std::shared_ptr<service_t> service(new service_t(name, *this, version));
        service->reconnect();
        return service;
    }

    template<class Service, typename... Args>
    future<std::shared_ptr<Service>>
    get_service_async(const std::string& name,
                      Args&&... args)
    {
        std::shared_ptr<service_t> service(new service_t(name, *this, Service::version));
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
        std::shared_ptr<service_t> service(new service_t(name, *this, version));
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

    const std::unique_ptr<service_t>&
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
    std::unique_ptr<service_t> m_resolver;
    std::shared_ptr<logger_t> m_logger;
};

template<class Event, typename... Args>
typename service_t::handler<Event>::future
service_t::call(Args&&... args) {
    if (m_channel) {
        std::lock_guard<std::mutex> lock(m_handlers_lock);

        auto h = std::make_shared<typename service_t::handler<Event>::type>();
        auto f = h->get_future();
        f.set_default_executor(m_manager.m_default_executor);
        m_channel->wr->write<Event>(m_session_counter, std::forward<Args>(args)...);
        m_handlers[m_session_counter] = h;
        ++m_session_counter;
        return f;
    } else {
        throw service_error_t(service_errc::not_connected);
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
    empty(future<std::shared_ptr<service_t>> &f) {
        // pass
    }

private:
    std::shared_ptr<service_t> m_service;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_HPP
