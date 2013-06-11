#ifndef COCAINE_FRAMEWORK_SERVICE_HPP
#define COCAINE_FRAMEWORK_SERVICE_HPP

#include <cocaine/framework/future.hpp>
#include <cocaine/framework/resolver.hpp>
#include <cocaine/framework/logging.hpp>
#include <cocaine/framework/service_error.hpp>

#include <cocaine/rpc/channel.hpp>
#include <cocaine/rpc/message.hpp>
#include <cocaine/rpc/protocol.hpp>
#include <cocaine/messages.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/asio/tcp.hpp>
#include <cocaine/asio/reactor.hpp>

#include <memory>
#include <string>
#include <functional>
#include <utility>
#include <mutex>

namespace cocaine { namespace framework {

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

    template<class ResultType>
    struct future_trait {
        typedef cocaine::framework::future<ResultType> future_type;
        typedef cocaine::framework::promise<ResultType> promise_type;
    };

    template<class... Args>
    struct future_trait<std::tuple<Args...>> {
        typedef cocaine::framework::future<Args...> future_type;
        typedef cocaine::framework::promise<Args...> promise_type;
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

            p.set_value(msg.get().as<ResultType>());
        } else if (message.id() == io::event_traits<io::rpc::error>::id) {
            int code;
            std::string msg;
            message.as<cocaine::io::rpc::error>(code, msg);
            p.set_exception(
                service_error_t(std::error_code(code, service_response_category()), msg)
            );
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
        } else if (message.id() == io::event_traits<io::rpc::error>::id) {
            int code;
            std::string msg;
            message.as<cocaine::io::rpc::error>(code, msg);
            p.set_exception(
                service_error_t(std::error_code(code, service_response_category()), msg)
            );
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

        template<class... Args>
        future_type
        get_future(Args&&... args) {
            return m_promise.get_future(std::forward<Args>(args)...);
        }

        void
        handle_message(const cocaine::io::message_t& message) {
            set_promise_result<result_type>(m_promise, message);
        }

    protected:
        promise_type m_promise;
    };

}} // detail::service

class service_t {
    COCAINE_DECLARE_NONCOPYABLE(service_t)

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
    service_t(const std::string& name,
              cocaine::io::reactor_t& working_ioservice,
              const executor_t& executor,
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

    cocaine::io::reactor_t& m_working_ioservice;
    executor_t m_default_executor;
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

    auto h = std::make_shared<typename service_t::handler<Event>::type>();
    auto f = h->get_future(m_default_executor);
    m_handlers[m_session_counter] = h;
    m_channel->wr->write<Event>(m_session_counter, std::forward<Args>(args)...);
    ++m_session_counter;
    return f;
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_HPP
