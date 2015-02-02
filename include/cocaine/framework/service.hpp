#pragma once

#include <cocaine/common.hpp>
#include <cocaine/idl/locator.hpp>
#include <cocaine/traits/endpoint.hpp>
#include <cocaine/traits/graph.hpp>
#include <cocaine/traits/tuple.hpp>
#include <cocaine/traits/vector.hpp>
#include <cocaine/idl/streaming.hpp>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/session.hpp"

namespace cocaine {

namespace framework {

template<
    class Event,
    class Upstream = typename io::event_traits<Event>::upstream_type,
    class Dispatch = typename io::event_traits<Event>::dispatch_type
>
struct invocation_result;

template<class Event, class T>
struct invocation_result<Event, io::primitive_tag<T>, void> {
    typedef typename boost::mpl::front<T>::type type;

    static
    future_t<type>
    apply(std::tuple<
              sender  <typename io::event_traits<Event>::dispatch_type>,
              receiver<typename io::event_traits<Event>::upstream_type>
          > ch)
    {
        auto rx = std::move(std::get<1>(ch));
        return rx.template recv<type>();
    }
};

template<class Event, class T, class D>
struct invocation_result<Event, io::streaming_tag<T>, io::streaming_tag<D>> {
    typedef typename boost::mpl::front<T>::type utype;
    typedef typename boost::mpl::front<D>::type dtype;

    typedef std::tuple<
        sender  <typename io::event_traits<Event>::dispatch_type>,
        receiver<typename io::event_traits<Event>::upstream_type>
    > type;

    static
    future_t<type>
    apply(std::tuple<
              sender  <typename io::event_traits<Event>::dispatch_type>,
              receiver<typename io::event_traits<Event>::upstream_type>
          > ch)
    {
        return make_ready_future<type>::value(ch);
    }
};

template<class T>
class service {
    std::string name;
    loop_t& loop;
    std::shared_ptr<session<T>> d;

    std::mutex mutex;

public:
    service(std::string name, loop_t& loop) :
        name(std::move(name)),
        loop(loop),
        d(std::make_shared<session<T>>(std::make_shared<basic_session_t>(loop)))
    {}

    ~service() {
        d->disconnect();
    }

    auto connect() -> future_t<void> {
        // TODO: Make async.
        std::lock_guard<std::mutex> lock(mutex);

        // Internally the session manages with connection state itself. On any network error it
        // should drop its internal state and return false.
        if (d->connected()) {
            return make_ready_future<void>::value();
        }

        try {
            CF_DBG("connecting to the locator");
            // TODO: Explicitly set Locator endpoints.
            const io_provider::ip::tcp::endpoint endpoint(io_provider::ip::tcp::v4(), 10053);
            auto locator = std::make_shared<session<io::locator_tag>>(std::make_shared<basic_session_t>(loop));
            try {
                locator->connect(endpoint).get();
                // TODO: Simplify that shit.
                CF_DBG("resolving");
                auto ch = locator->template invoke<io::locator::resolve>(name).get();
                auto rx = std::move(std::get<1>(ch));
                typedef std::tuple<
                    std::vector<asio::ip::tcp::endpoint>,
                    unsigned int,
                    io::graph_basis_t
                > resolve_result;

                auto result = rx.template recv<resolve_result>().get();
                CF_DBG("resolving - done");
                // TODO: Check version.
                locator->disconnect();

                auto endpoints = std::get<0>(result);

                d->connect(endpoints[0]).get();
                CF_DBG("connecting - done");
            } catch (...) {
                locator->disconnect();
                throw;
            }
            return make_ready_future<void>::value();
        } catch (const std::runtime_error& err) {
            return make_ready_future<void>::error(err);
        }
    }

    template<class Event, class... Args>
    future_t<typename invocation_result<Event>::type>
    invoke(Args&&... args) {
        typedef typename invocation_result<Event>::type result_type;

        try {
            // TODO: Make asyncronous call through `then`.
            connect().get();

            auto ch = d->template invoke<Event>(std::forward<Args>(args)...).get();
            return invocation_result<Event>::apply(ch);
        } catch (const std::exception& err) {
            return make_ready_future<result_type>::error(err);
        }
    }
};

} // namespace framework

} // namespace cocaine
