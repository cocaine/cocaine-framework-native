#ifndef COCAINE_FRAMEWORK_FUTURE_DETAIL_HPP
#define COCAINE_FRAMEWORK_FUTURE_DETAIL_HPP

#include <cocaine/framework/future/shared_state.hpp>
#include <cocaine/framework/future/variant.hpp>

#include <memory>

namespace cocaine { namespace framework {

template<class... Args>
class future;

namespace detail { namespace future {

template<class... Args>
struct future_traits {
    typedef variant<std::shared_ptr<shared_state<Args...>>, ready_state<Args...>>
            state_type;
};

/*
 * Function to create future from state (used by promise and make_ready_future).
 */
template<class... Args>
inline
cocaine::framework::future<Args...>
future_from_state(typename future_traits<Args...>::state_type&& state,
                  executor_t executor)
{
    return cocaine::framework::future<Args...>(std::move(state), executor);
}

template<class... Args>
inline
cocaine::framework::future<Args...>
future_from_state(std::shared_ptr<shared_state<Args...>> state,
                  executor_t executor = executor_t())
{
    typename future_traits<Args...>::state_type s;
    s.template set<0>(std::move(state));
    return future_from_state<Args...>(std::move(s), executor);
}

template<class... Args>
inline
cocaine::framework::future<Args...>
future_from_state(ready_state<Args...>&& state,
                  executor_t executor = executor_t())
{
    typename future_traits<Args...>::state_type s;
    s.template set<1>(std::move(state));
    return future_from_state<Args...>(std::move(s), executor);
}

/*
 * Visitors to use future::state_type
 */
template<class... Args>
struct wait_visitor {
    typedef void result_type;

    template<unsigned int>
    void
    visit(const std::shared_ptr<shared_state<Args...>>& state) const {
        state->wait();
    }

    template<unsigned int>
    void
    visit(const ready_state<Args...>& state) const {
        state.wait();
    }
};

template<class Rep, class Period, class... Args>
struct wait_for_visitor {
    typedef void result_type;

    wait_for_visitor(const std::chrono::duration<Rep, Period>& rel_time) :
        m_rel_time(rel_time)
    {
        // pass
    }

    template<unsigned int>
    void
    visit(const std::shared_ptr<shared_state<Args...>>& state) const {
        state->wait_for(m_rel_time);
    }

    template<unsigned int>
    void
    visit(const ready_state<Args...>& state) const {
        state.wait_for(m_rel_time);
    }

private:
    const std::chrono::duration<Rep, Period>& m_rel_time;
};

template<class Clock, class Duration, class... Args>
struct wait_until_visitor {
    typedef void result_type;

    wait_until_visitor(const std::chrono::time_point<Clock, Duration>& timeout) :
        m_timeout(timeout)
    {
        // pass
    }

    template<unsigned int>
    void
    visit(const std::shared_ptr<shared_state<Args...>>& state) const {
        state->wait_until(m_timeout);
    }

    template<unsigned int>
    void
    visit(const ready_state<Args...>& state) const {
        state.wait_until(m_timeout);
    }

private:
    const std::chrono::time_point<Clock, Duration>& m_timeout;
};

template<class... Args>
struct ready_visitor {
    typedef bool result_type;

    template<unsigned int>
    result_type
    visit(const std::shared_ptr<shared_state<Args...>>& state) const {
        return state->ready();
    }

    template<unsigned int>
    result_type
    visit(const ready_state<Args...>& state) const {
        return state.ready();
    }
};


// helper to get content of shared_state
template<class... Args>
struct getter {
    typedef typename cocaine::framework::future_traits<Args...>::storable_type
            result_type;

    template<class State>
    static
    inline
    result_type
    get(State& state) {
        return std::move(state.get());
    }
};

template<>
struct getter<void> {
    typedef void result_type;

    template<class State>
    static
    inline
    result_type
    get(State& state) {
        state.get();
    }
};

template<class... Args>
struct get_visitor {
    typedef typename getter<Args...>::result_type result_type;

    template<unsigned int>
    result_type
    visit(std::shared_ptr<shared_state<Args...>>& state) {
        return getter<Args...>::get(*state);
    }

    template<unsigned int>
    result_type
    visit(ready_state<Args...>& state) {
        return getter<Args...>::get(state);
    }
};


// helper to provide unwrapping of futures
template<class Future>
struct unwrapper;

template<class... Args>
struct unwrapper<cocaine::framework::future<Args...>> {
    typedef cocaine::framework::future<Args...>
            unwrapped_type;

    static
    inline
    unwrapped_type
    unwrap(cocaine::framework::future<Args...>&& fut) {
        return std::move(fut);
    }
};

template<class... Args>
struct unwrapper<cocaine::framework::future<cocaine::framework::future<Args...>>> {
    typedef cocaine::framework::future<Args...>
            unwrapped_type;

    struct helper2 {
        helper2(std::shared_ptr<shared_state<Args...>> new_state) :
            m_new_state(new_state)
        {
            // pass
        }

        void
        operator()(cocaine::framework::future<Args...>& fut) {
            try {
                m_new_state->set_value(fut.get());
            } catch (...) {
                m_new_state->set_exception(std::current_exception());
            }
        }

    private:
        std::shared_ptr<shared_state<Args...>> m_new_state;
    };

    struct helper1 {
        helper1(std::shared_ptr<shared_state<Args...>> new_state) :
            m_new_state(new_state)
        {
            // pass
        }

        void
        operator()(cocaine::framework::future<cocaine::framework::future<Args...>>& fut) {
            try {
                fut.get().then(executor_t(), helper2(m_new_state));
            } catch (...) {
                m_new_state->set_exception(std::current_exception());
            }
        }

    private:
        std::shared_ptr<shared_state<Args...>> m_new_state;
    };

    static
    inline
    unwrapped_type
    unwrap(cocaine::framework::future<cocaine::framework::future<Args...>>&& fut) {
        if (fut.ready()) {
            executor_t executor = fut.get_default_executor();
            try {
                auto f = fut.get();
                f.set_default_executor(executor);
                return f;
            } catch (...) {
                return future_from_state<Args...>(std::current_exception(), executor);
            }
        } else {
            auto new_state = std::make_shared<shared_state<Args...>>();
            executor_t executor = fut.get_default_executor();
            fut.then(executor_t(), helper1(new_state));
            return future_from_state<Args...>(new_state, executor);
        }
    }
};

// Helper to declare 'then' result type.
// It seems that g++ 4.4 doesn't understand typename
// unwrapper<future<decltype(declval<F>()(declval<Args>()...))>>::unwrapped_type
// written directly in signature of function.
template<class F, class... Args>
struct unwrapped_result {
    typedef decltype(declval<F>()(declval<Args>()...))
            result_type;
    typedef typename unwrapper<cocaine::framework::future<result_type>>::unwrapped_type
            type;
};

/*
 * Implementation of 'then' method.
 */

template<class Result, class... Args>
struct task_caller {
    static
    void
    call(std::shared_ptr<shared_state<Result>> state,
         std::function<Result(Args...)> f,
         Args&&... args)
    {
        try {
            state->set_value(f(std::forward<Args>(args)...));
        } catch (...) {
            state->set_exception(std::current_exception());
        }
    }
};

template<class... Args>
struct task_caller<void, Args...> {
    static
    void
    call(std::shared_ptr<shared_state<void>> state,
         std::function<void(Args...)> f,
         Args&&... args)
    {
        try {
            f(std::forward<Args>(args)...);
            state->set_value();
        } catch (...) {
            state->set_exception(std::current_exception());
        }
    }
};

// Helper to call 'then' callback with future. It stores future in heap to be copyable.
template<class Result, class Future>
struct continuation_caller {
    template<class F>
    continuation_caller(F&& callback,
                        Future&& f) :
        m_callback(std::forward<F>(callback)),
        m_future(new Future(std::move(f)))
    {
        // pass
    }

    Future&
    get_future() const {
        return *m_future;
    }

    Result
    operator()(Future& f) {
        return m_callback(f);
    }

private:
    std::function<Result(Future&)> m_callback;
    std::shared_ptr<Future> m_future;
};

template<class F, class Future>
inline
typename std::enable_if<
    std::is_same<typename unwrapped_result<F, Future&>::result_type,
                 void>::value,
    cocaine::framework::future<typename unwrapped_result<F, Future&>::result_type>
>::type
ready_from_task(F&& task,
                Future& f)
{
    task(f);
    return future_from_state(ready_state<typename unwrapped_result<F, Future&>::result_type>(0));
}

template<class F, class Future>
inline
typename std::enable_if<
    !std::is_same<typename unwrapped_result<F, Future&>::result_type,
                  void>::value,
    cocaine::framework::future<typename unwrapped_result<F, Future&>::result_type>
>::type
ready_from_task(F&& task,
                Future& f)
{
    return future_from_state(ready_state<typename unwrapped_result<F, Future&>::result_type>(0, task(f)));
}

}} // namespace detail::future

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_FUTURE_DETAIL_HPP
