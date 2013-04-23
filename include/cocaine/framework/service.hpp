#ifndef COCAINE_FRAMEWORK_SERVICE_HPP
#define COCAINE_FRAMEWORK_SERVICE_HPP

#include <cocaine/framework/resolver.hpp>
#include <cocaine/framework/logging.hpp>

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
#include <functional>
#include <mutex>

namespace cocaine { namespace framework {

class service_error_t :
    public std::runtime_error
{
public:
    explicit service_error_t(const std::string& what) :
        std::runtime_error(what)
    {
        // pass
    }
};

namespace detail { namespace service {

    struct service_handler_concept_t {
        virtual
        ~service_handler_concept_t() {
            // pass
        }

        virtual
        void
        handle_message(const cocaine::io::message_t&) = 0;
    };

    template<class Result, class = void>
    struct message_handler {
        typedef std::function<void(const Result&)> type;

        struct empty_handler {
            void
            operator()(const Result&)
            {
                // pass
            }
        };

        template<class F>
        static inline
        void
        apply(const F& fun,
              const msgpack::object& message)
        {
            fun(message.as<Result>());
        }
    };

    template<class Result>
    struct message_handler<
            Result,
            typename std::enable_if<
                boost::mpl::is_sequence<Result>::value
            >::type
        >
    {
        typedef typename cocaine::basic_slot<void, Result>::callable_type
                type;

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

        typedef typename construct_functor<
                    typename boost::mpl::begin<Result>::type,
                    typename boost::mpl::end<Result>::type
                >::type
                empty_handler;

        template<class F>
        static inline
        void
        apply(const F& fun,
              const msgpack::object& message)
        {
            cocaine::invoke<Result>::apply(fun, message);
        }
    };

    template<class Event,
             class Result = typename cocaine::io::event_traits<Event>::result_type>
    struct handler_detail
    {
        typedef typename message_handler<Result>::type message_handler_t;

        handler_detail() :
            m_message_handler(typename message_handler<Result>::empty_handler())
        {
            // pass
        }

        void
        handle_chunk(const msgpack::object& message) {
            message_handler<Result>::apply(m_message_handler, message);
        }

        void
        on_message(message_handler_t handler) {
            m_message_handler = handler;
        }

        message_handler_t m_message_handler;
    };

    template<class Event>
    struct handler_detail<Event, void>
    {
        void
        handle_chunk(const msgpack::object& message) {
            // pass
        }
    };

    template<class Event>
    class service_handler;

    template<class Event, class Result = typename cocaine::io::event_traits<Event>::result_type>
    struct future {
        typedef service_handler<Event> handler_t;

        future(std::shared_ptr<handler_t> promise) :
            m_promise(promise)
        {
            // pass
        }

        future&
        on_message(typename handler_t::message_handler_t handler) {
            m_promise->on_message(handler);
            return *this;
        }

        future&
        on_error(typename handler_t::error_handler_t handler) {
            m_promise->on_error(handler);
            return *this;
        }

    private:
        std::shared_ptr<handler_t> m_promise;
    };

    template<class Event>
    struct future<Event, void> {
        typedef service_handler<Event> handler_t;

        future(std::shared_ptr<handler_t> promise) :
            m_promise(promise)
        {
            // pass
        }

        future&
        on_error(typename handler_t::error_handler_t handler) {
            m_promise->on_error(handler);
            return *this;
        }

    private:
        std::shared_ptr<handler_t> m_promise;
    };

    template<class Event>
    class service_handler :
        public service_handler_concept_t,
        protected handler_detail<Event>
    {
        COCAINE_DECLARE_NONCOPYABLE(service_handler)

        friend class future<Event>;

        typedef std::function<void(cocaine::error_code, const std::string&)>
                error_handler_t;

        struct ignore_error_t {
            void
            operator()(cocaine::error_code,
                       const std::string&)
            {
                // pass
            }
        };

    public:
        service_handler() :
            handler_detail<Event>(),
            m_error_handler(ignore_error_t())
        {
            // pass
        }

        void
        handle_message(const cocaine::io::message_t&);

    protected:
        void
        on_error(error_handler_t handler) {
            m_error_handler = handler;
        }

    protected:
        error_handler_t m_error_handler;
    };

    template<class Event>
    void
    service_handler<Event>::handle_message(const cocaine::io::message_t& message) {
        if (message.id() == io::event_traits<io::rpc::chunk>::id) {
            std::string data;
            message.as<cocaine::io::rpc::chunk>(data);
            msgpack::unpacked msg;
            msgpack::unpack(&msg, data.data(), data.size());

            this->handle_chunk(msg.get());
        } else if (message.id() == io::event_traits<io::rpc::error>::id) {
            cocaine::error_code code;
            std::string msg;
            message.as<cocaine::io::rpc::error>(code, msg);
            m_error_handler(code, msg);
        }
    }

}} // detail::service

class service_t {
    COCAINE_DECLARE_NONCOPYABLE(service_t)

public:
    template<class Event>
    struct handler {
        typedef detail::service::service_handler<Event> type;
        typedef detail::service::future<Event> future;
    };

public:
    service_t(const std::string& name,
              cocaine::io::reactor_t& ioservice,
              const cocaine::io::tcp::endpoint& resolver_endpoint,
              std::shared_ptr<logger_t> logger,
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

    typedef uint64_t session_id_t;

    typedef std::map<session_id_t, std::shared_ptr<detail::service::service_handler_concept_t>>
            handlers_map_t;

private:
    void
    connect();

    void
    on_message(const cocaine::io::message_t& message);

private:
    std::string m_name;
    unsigned int m_version;
    std::pair<std::string, uint16_t> m_endpoint;

    cocaine::io::reactor_t& m_ioservice;
    std::shared_ptr<resolver_t> m_resolver;
    std::shared_ptr<logger_t> m_logger;
    std::shared_ptr<iochannel_t> m_channel;

    session_id_t m_session_counter;
    handlers_map_t m_handlers;
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

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_HPP
