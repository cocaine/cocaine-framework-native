#ifndef COCAINE_FRAMEWORK_SERVICE_HPP
#define COCAINE_FRAMEWORK_SERVICE_HPP

#include <cocaine/framework/generator.hpp>
#include <cocaine/framework/service_error.hpp>

#include <cocaine/rpc/channel.hpp>
#include <cocaine/rpc/message.hpp>
#include <cocaine/rpc/protocol.hpp>
#include <cocaine/traits/typelist.hpp>
#include <cocaine/traits/tuple.hpp>
#include <cocaine/messages.hpp>

#include <memory>
#include <string>

namespace cocaine { namespace framework {

class service_connection_t;

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
            p.write(std::move(r));
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
            p.write(std::move(data));
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
                p.error(cocaine::framework::make_exception_ptr(
                    service_error_t(std::error_code(code, service_response_category()), msg)
                ));
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
            if (message.id() == io::event_traits<io::rpc::error>::id) {
                int code;
                std::string msg;
                message.as<cocaine::io::rpc::error>(code, msg);
                p.error(cocaine::framework::make_exception_ptr(
                    service_error_t(std::error_code(code, service_response_category()), msg)
                ));
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
            m_promise.error(e);
        }

    protected:
        promise_type m_promise;
    };

}} // detail::service

template<class Event>
struct service_traits {
    typedef typename detail::service::service_handler<Event>::future_type
            future_type;

    typedef typename detail::service::service_handler<Event>::promise_type
            promise_type;
};

enum class service_status {
    disconnected,
    connecting,
    connected,
    not_found
};

class service_t {
    COCAINE_DECLARE_NONCOPYABLE(service_t)

public:
    service_t(std::shared_ptr<service_connection_t> connection);

    virtual
    ~service_t();

    std::string
    name();

    service_status
    status();

    void
    set_timeout(float timeout);

    template<class Event, typename... Args>
    typename service_traits<Event>::future_type
    call(Args&&... args);

    void
    reconnect();

    future<void>
    async_reconnect();

    void
    soft_destroy();

private:
    std::shared_ptr<service_connection_t>
    connection();

private:
    std::weak_ptr<service_connection_t> m_connection;
};

}} // namespace cocaine::framework

#include <cocaine/framework/service_manager.hpp>
#include <cocaine/framework/service_impl.hpp>

template<class Event, typename... Args>
typename cocaine::framework::service_traits<Event>::future_type
cocaine::framework::service_t::call(Args&&... args) {
    return connection()->call<Event, Args...>(std::forward<Args>(args)...);
}

#endif // COCAINE_FRAMEWORK_SERVICE_HPP
