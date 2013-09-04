#ifndef COCAINE_FRAMEWORK_FUTURE_PROMISE_HPP
#define COCAINE_FRAMEWORK_FUTURE_PROMISE_HPP

#include <cocaine/framework/future/future.hpp>

namespace cocaine { namespace framework {

namespace detail { namespace future {

    template<class... Args>
    class state_handler {
        COCAINE_DECLARE_NONCOPYABLE(state_handler)
    public:
        state_handler() :
            m_state(new shared_state<Args...>()),
            m_future_retrieved(false)
        {
            // pass
        }

        ~state_handler() {
            m_state->try_set_exception(
                cocaine::framework::make_exception_ptr(future_error(future_errc::broken_promise))
            );
        }

        cocaine::framework::future<Args...>
        get_future() {
            bool retrieved = m_future_retrieved.exchange(true);
            if (!retrieved) {
                return future_from_state<Args...>(m_state);
            } else {
                throw future_error(future_errc::future_already_retrieved);
            }
        }

        std::shared_ptr<shared_state<Args...>>
        state() const {
            return m_state;
        }

    protected:
        std::shared_ptr<shared_state<Args...>> m_state;
        std::atomic<bool> m_future_retrieved;
    };

}} // namespace detail::future

template<class... Args>
class promise {
public:
    typedef future<Args...>
            future_type;

    promise() :
        m_state(new detail::future::state_handler<Args...>())
    {
        // pass
    }

    void
    set_exception(std::exception_ptr e) {
        state()->set_exception(e);
    }

    template<class Exception>
    void
    set_exception(const Exception& e) {
        state()->set_exception(
            cocaine::framework::make_exception_ptr(e)
        );
    }

    template<class... Args2>
    void
    set_value(Args2&&... args) {
        state()->set_value(std::forward<Args2>(args)...);
    }

    future<Args...>
    get_future() {
        check_state();
        return m_state->get_future();
    }

protected:
    std::shared_ptr<detail::future::shared_state<Args...>>
    state() const {
        check_state();
        return m_state->state();
    }

    void
    check_state() const {
        if (!m_state) {
            throw future_error(future_errc::no_state);
        }
    }

    explicit
    promise(const std::shared_ptr<detail::future::state_handler<Args...>>& state) :
        m_state(state)
    {
        // pass
    }

private:
    std::shared_ptr<detail::future::state_handler<Args...>> m_state;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_FUTURE_PROMISE_HPP
