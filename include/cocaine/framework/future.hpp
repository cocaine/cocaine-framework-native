#ifndef COCAINE_FRAMEWORK_FUTURE_HPP
#define COCAINE_FRAMEWORK_FUTURE_HPP

#include <cocaine/framework/future_error.hpp>

#include <cocaine/common.hpp>

#if defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ == 4
    #include <cstdatomic>
#else
    #include <atomic>
#endif

#include <functional>
#include <memory>
#include <tuple>
#include <ios>
#include <string>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <exception>

#if !defined(HAVE_GCC46) && !defined(nullptr)
#define nullptr ((void*)0)
#endif

namespace cocaine { namespace framework {

typedef std::function<void(std::function<void()>)> executor_t;

template<class ... Args>
struct promise;
template<class F>
struct packaged_task;
template<class ... Args>
struct future;

namespace detail { namespace future {

    template<class Exception>
    std::exception_ptr
    make_exception_ptr(const Exception& e) {
        try {
            throw e;
        } catch (...) {
            return std::current_exception();
        }
    }

    template<class T>
    T&&
    declval();

    template<class ... Args>
    struct promise_base;

    // "Base" types are only needed to reduce duplication of code in templates specializations.

    template<class... Args>
    struct shared_state_base {
        shared_state_base() :
            m_is_ready(false)
        {
            // pass
        }

        void
        set_exception(std::exception_ptr e) {
            std::unique_lock<std::mutex> lock(m_ready_mutex);
            if (!ready()) {
                m_exception = e;
                make_ready(lock);
            } else {
                throw future_error(future_errc::promise_already_satisfied);
            }
        }

        void
        try_set_exception(std::exception_ptr e) {
            std::unique_lock<std::mutex> lock(m_ready_mutex);
            if (!ready()) {
                m_exception = e;
                make_ready(lock);
            }
        }

        void
        wait() {
            std::unique_lock<std::mutex> lock(m_ready_mutex);
            while (!ready()) {
                m_ready.wait(lock);
            }
        }

        template<class Rep, class Period>
        void
        wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
            std::unique_lock<std::mutex> lock(m_ready_mutex);
            m_ready.wait_for(lock, rel_time, std::bind(&shared_state_base::ready, this));
        }

        template<class Clock, class Duration>
        void
        wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) {
            std::unique_lock<std::mutex> lock(m_ready_mutex);
            m_ready.wait_until(lock, timeout_time, std::bind(&shared_state_base::ready, this));
        }

        bool
        ready() const {
            return m_is_ready;
        }

        template<class F>
        void
        set_callback(F&& callback) {
            std::unique_lock<std::mutex> lock(m_ready_mutex);
            if (this->ready()) {
                lock.unlock();
                callback();
            } else {
                m_callback = std::forward<F>(callback);
            }
        }

    protected:
        void
        make_ready(std::unique_lock<std::mutex>& lock) {
            m_is_ready = true;
            m_ready.notify_all();
            lock.unlock();
            if (m_callback) {
                m_callback();
                m_callback = std::function<void()>();
            }
        }

    protected:
        std::exception_ptr m_exception;
        std::function<void()> m_callback;
        std::atomic<bool> m_is_ready;
        std::mutex m_ready_mutex;
        std::condition_variable m_ready;
    };

    // shared state of promise-future
    // it's a "core" of futures, while "future" and "promise" are just wrappers to access shared state
    template<class... Args>
    struct shared_state :
        public shared_state_base<Args...>
    {
        typedef std::tuple<Args...> value_type;

        template<class... Args2>
        void
        set_value(Args2&&... args) {
            std::unique_lock<std::mutex> lock(this->m_ready_mutex);
            if (!this->ready()) {
                this->m_result.reset(new value_type(std::forward<Args2>(args)...));
                this->make_ready(lock);
            } else {
                throw future_error(future_errc::promise_already_satisfied);
            }
        }

        template<class... Args2>
        void
        try_set_value(Args2&&... args) {
            std::unique_lock<std::mutex> lock(this->m_ready_mutex);
            if (!this->ready()) {
                this->m_result.reset(new value_type(std::forward<Args2>(args)...));
                this->make_ready(lock);
            }
        }

        value_type&
        get() {
            this->wait();
            if (this->m_exception != std::exception_ptr()) {
                std::rethrow_exception(this->m_exception);
            } else {
                return *m_result;
            }
        }

    private:
        std::unique_ptr<value_type> m_result; // instead of boost::optional that can not into move semantic
    };

    template<>
    struct shared_state<void> :
        public shared_state_base<void>
    {
        void
        set_value() {
            std::unique_lock<std::mutex> lock(this->m_ready_mutex);
            if (!this->ready()) {
                this->make_ready(lock);
            } else {
                throw future_error(future_errc::promise_already_satisfied);
            }
        }

        void
        try_set_value() {
            std::unique_lock<std::mutex> lock(this->m_ready_mutex);
            if (!this->ready()) {
                this->make_ready(lock);
            }
        }

        void
        get() {
            this->wait();
            if (this->m_exception != std::exception_ptr()) {
                std::rethrow_exception(this->m_exception);
            }
        }
    };

    // future base types
    // they are needed to avoid code duplication in template specializations
    template<class ... Args>
    struct future_base {
        COCAINE_DECLARE_NONCOPYABLE(future_base)

        bool
        valid() const {
            return static_cast<bool>(m_state);
        }

        void
        wait() const {
            check_state();
            m_state->wait();
        }

        template<class Rep, class Period>
        void
        wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
            check_state();
            m_state->wait(rel_time);
        }

        template<class Clock, class Duration>
        void
        wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) const {
            check_state();
            m_state->wait(timeout_time);
        }

        bool
        ready() const {
            check_state();
            return m_state->ready();
        }

        void
        set_default_executor(const executor_t& executor) {
            m_executor = executor;
        }

    protected:
        typedef std::shared_ptr<shared_state<Args...>> state_type;

        explicit future_base(state_type state = state_type(),
                             executor_t executor = executor_t()):
            m_state(state),
            m_executor(executor)
        {
            // pass
        }

        future_base(future_base&& other) :
            m_state(std::move(other.m_state)),
            m_executor(std::move(other.m_executor))
        {
            // pass
        }

        void
        operator=(future_base&& other) {
            m_state = std::move(other.m_state);
            m_executor = std::move(other.m_executor);
        }

        void
        invalidate() {
            m_state.reset();
        }

        void
        check_state() const {
            if (!valid()) {
                throw future_error(future_errc::no_state);
            }
        }

    protected:
        state_type m_state;
        executor_t m_executor;
    };

    // helper to get content of shared_state
    template<class... Args>
    struct getter {
        typedef std::tuple<Args...> result_type;

        template<class State>
        static
        inline
        result_type
        get(State& state) {
            return std::move(state.get());
        }
    };

    template<class T>
    struct getter<T> {
        typedef T result_type;

        template<class State>
        static
        inline
        result_type
        get(State& state) {
            return std::move(std::get<0>(state.get()));
        }
    };

    template<class T>
    struct getter<T&> {
        typedef T& result_type;

        template<class State>
        static
        inline
        result_type
        get(State& state) {
            return std::get<0>(state.get());
        }
    };

    template<class... Args>
    struct yet_another_future_base :
        public future_base<Args...>
    {
        yet_another_future_base() {
            // pass
        }

        yet_another_future_base(yet_another_future_base&& other) :
            future_base<Args...>(std::move(other))
        {
            // pass
        }

        typename detail::future::getter<Args...>::result_type
        get() {
            this->check_state();
            auto state = this->m_state;
            this->invalidate();
            return detail::future::getter<Args...>::get(*state);
        }
    protected:
        explicit yet_another_future_base(typename future_base<Args...>::state_type state,
                                         const executor_t& executor = executor_t()) :
            future_base<Args...>(state, executor)
        {
            // pass
        }
    };

    template<>
    struct yet_another_future_base<void> :
        public future_base<void>
    {
        yet_another_future_base() {
            // pass
        }

        yet_another_future_base(yet_another_future_base&& other) :
            future_base<void>(std::move(other))
        {
            // pass
        }

        void
        get() {
            this->check_state();
            auto state = this->m_state;
            this->invalidate();
            state->get();
        }
    protected:
        explicit yet_another_future_base(future_base<void>::state_type state,
                                         const executor_t& executor) :
            future_base<void>(state, executor)
        {
            // pass
        }
    };

    // create future from shared_state
    template<class... Args>
    inline
    cocaine::framework::future<Args...>
    future_from_state(std::shared_ptr<shared_state<Args...>>& state,
                      executor_t executor = executor_t())
    {
        return cocaine::framework::future<Args...>(state, executor);
    }

    // promise basic type
    // packaged_task also inherits it
    template<class... Args>
    struct promise_base {
        COCAINE_DECLARE_NONCOPYABLE(promise_base)

        promise_base() :
            m_state(new shared_state<Args...>()),
            m_future(future_from_state<Args...>(m_state))
        {
            // pass
        }

        promise_base(promise_base&& other) :
            m_state(std::move(other.m_state)),
            m_future(std::move(other.m_future))
        {
            // pass
        }

        ~promise_base() {
            if (m_state) {
                m_state->try_set_exception(
                    cocaine::framework::detail::future::make_exception_ptr(future_error(future_errc::broken_promise))
                );
            }
        }

        void
        operator=(promise_base&& other) {
            m_state = std::move(other.m_state);
            m_future = std::move(other.m_future);
        }

        void
        set_exception(std::exception_ptr e) {
            if (m_state) {
                m_state->set_exception(e);
            } else {
                throw future_error(future_errc::no_state);
            }
        }

        template<class Exception>
        void
        set_exception(const Exception& e) {
            if (m_state) {
                m_state->set_exception(
                    cocaine::framework::detail::future::make_exception_ptr(e)
                );
            } else {
                throw future_error(future_errc::no_state);
            }
        }

        cocaine::framework::future<Args...>
        get_future() {
            if (!m_future.valid()) {
                if (m_state) {
                    throw future_error(future_errc::future_already_retrieved);
                } else {
                    throw future_error(future_errc::no_state);
                }
            } else {
                return std::move(m_future);
            }
        }

    protected:
        explicit
        promise_base(std::shared_ptr<shared_state<Args...>> state) :
            m_state(state),
            m_future(future_from_state<Args...>(m_state))
        {
            // pass
        }

        std::shared_ptr<shared_state<Args...>> m_state;
        cocaine::framework::future<Args...> m_future;
    };

    // helpers to provide unwrapping of futures
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
                    m_new_state->set_value(std::move(fut.get()));
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
                    fut.get().then(helper2(m_new_state));
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
            auto new_state = std::make_shared<shared_state<Args...>>();
            auto executor = fut.m_executor;
            fut.then(helper1(new_state));
            return future_from_state<Args...>(new_state, executor);
        }
    };

    // Helper to declare 'then' result type.
    // It seems that g++ 4.4 doesn't understand
    // typename unwrapper<future<decltype(declval<F>()(declval<Args>()...))>>::unwrapped_type
    // written directly in signature of function.
    template<class F, class... Args>
    struct unwrapped_result {
        typedef decltype(declval<F>()(declval<Args>()...)) result_type;
        typedef typename unwrapper<cocaine::framework::future<result_type>>::unwrapped_type type;
    };

    // helper to call packed task
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

        Result
        operator()() {
            return m_callback(*m_future);
        }

    private:
        std::function<Result(Future&)> m_callback;
        std::shared_ptr<Future> m_future;
    };

    template<class Future>
    struct continuation_caller<void, Future> {
        template<class F>
        continuation_caller(F&& callback,
                            Future&& f) :
            m_callback(std::forward<F>(callback)),
            m_future(new Future(std::move(f)))
        {
            // pass
        }

        void
        operator()() {
            m_callback(*m_future);
        }

    private:
        std::function<void(Future&)> m_callback;
        std::shared_ptr<Future> m_future;
    };

}} // namespace detail::future

template<class... Args>
class future :
    public detail::future::yet_another_future_base<Args...>
{
    friend future<Args...>
           detail::future::future_from_state<Args...>(
               std::shared_ptr<shared_state<Args...>>& state,
               executor_t executor
           );

    explicit future(typename detail::future::future_base<Args...>::state_type state,
                    const executor_t& executor) :
        detail::future::yet_another_future_base<Args...>(state, executor)
    {
        // pass
    }

public:
    future() {
        // pass
    }

    future(future&& other) :
        detail::future::yet_another_future_base<Args...>(std::move(other))
    {
        // pass
    }

    future(const future&) = delete;

    future&
    operator=(future&& other) {
        detail::future::future_base<Args...>::operator=(std::move(other));
        return *this;
    }

    typename detail::future::unwrapper<future<Args...>>::unwrapped_type
    unwrap() {
        return detail::future::unwrapper<future<Args...>>::unwrap(std::move(*this));
    }

    template<class F>
    typename detail::future::unwrapped_result<F, future<Args...>&>::type
    then(executor_t executor,
         F&& callback);

    template<class F>
    typename detail::future::unwrapped_result<F, future<Args...>&>::type
    then(F&& callback) {
        return this->then(this->m_executor, std::forward<F>(callback));
    }
};

template<class... Args>
template<class F>
typename detail::future::unwrapped_result<F, future<Args...>&>::type
future<Args...>::then(executor_t executor,
                      F&& callback)
{
    this->check_state();

    typedef decltype(callback(*this)) result_type;

    auto old_state = this->m_state;
    auto new_state = std::make_shared<detail::future::shared_state<result_type>>();
    auto task = std::bind(detail::future::task_caller<result_type>::call,
                          new_state,
                          detail::future::continuation_caller<result_type, future<Args...>>(
                              std::forward<F>(callback),
                              std::move(*this)
                          ));

    if (executor) {
        old_state->set_callback(std::bind(executor, std::function<void()>(task))); // why i can't write just task?
    } else {
        old_state->set_callback(std::function<void()>(task));
    }

    return detail::future::future_from_state<result_type>(new_state).unwrap();
}

template<class... Args>
struct promise :
    public detail::future::promise_base<Args...>
{
    typedef future<Args...> future_type;

    promise()
    {
        // pass
    }

    promise(promise&& other) :
        detail::future::promise_base<Args...>(std::move(other))
    {
        // pass
    }

    promise&
    operator=(promise&& other) {
        detail::future::promise_base<Args...>::operator=(std::move(other));
        return *this;
    }

    template<class... Args2>
    void
    set_value(Args2&&... args) {
        if (this->m_state) {
            this->m_state->set_value(std::forward<Args2>(args)...);
        } else {
            throw future_error(future_errc::no_state);
        }
    }
};

template<>
struct promise<void> :
    public detail::future::promise_base<void>
{
    typedef future<void> future_type;

    promise()
    {
        // pass
    }

    promise(promise&& other) :
        detail::future::promise_base<void>(std::move(other))
    {
        // pass
    }

    promise&
    operator=(promise&& other) {
        detail::future::promise_base<void>::operator=(std::move(other));
        return *this;
    }

    void
    set_value() {
        if (this->m_state) {
            this->m_state->set_value();
        } else {
            throw future_error(future_errc::no_state);
        }
    }
};

template<class T>
struct packaged_task;

template<class R, class... Args>
struct packaged_task<R(Args...)> :
    public detail::future::promise_base<R>
{
    packaged_task() :
        detail::future::promise_base<R>(std::shared_ptr<detail::future::shared_state<R>>())
    {
        // pass
    }

    template<class F>
    explicit
    packaged_task(F&& f) :
        m_function(std::forward<F>(f))
    {
        // pass
    }

    template<class F>
    packaged_task(executor_t executor,
                  F&& f) :
        m_function(std::forward<F>(f)),
        m_executor(executor)
    {
        // pass
    }

    packaged_task(packaged_task&& other) :
        detail::future::promise_base<R>(std::move(other)),
        m_function(std::move(other.m_function)),
        m_executor(std::move(other.m_executor))
    {
        // pass
    }

    packaged_task&
    operator=(packaged_task&& other) {
        m_function = std::move(other.m_function);
        m_executor = std::move(other.m_executor);
        detail::future::promise_base<R>::operator=(std::move(other));
        return *this;
    }

    bool
    valid() const {
        return static_cast<bool>(m_function);
    }

    void
    reset() {
        this->m_state.reset(new detail::future::shared_state<R>());
        this->m_retrieved = false;
    }

    typename detail::future::unwrapper<cocaine::framework::future<R>>::unwrapped_type
    get_future() {
        auto f = detail::future::promise_base<R>::get_future().unwrap();
        f.set_default_executor(m_executor);
        return f;
    }

    template<class... Args2>
    void
    operator()(Args2&&... args) {
        if (this->m_state) {
            if (m_executor) {
                m_executor(std::bind(detail::future::task_caller<R, Args...>::call,
                                     this->m_state,
                                     m_function,
                                     std::forward<Args2>(args)...));
            } else {
                detail::future::task_caller<R, Args...>::call(this->m_state,
                                                              m_function,
                                                              std::forward<Args2>(args)...);
            }
            this->m_state.reset();
        } else {
            throw future_error(future_errc::no_state);
        }
    }

private:
    using detail::future::promise_base<R>::set_exception;

    std::function<R(Args...)> m_function;
    executor_t m_executor;
};

template<class... Args>
struct make_ready_future {
    template<class... Args2>
    static
    typename promise<Args...>::future_type
    make(Args2&&... args) {
        promise<Args...> p;
        p.set_value(std::forward<Args2>(args)...);
        return p.get_future();
    }

    template<class... Args2>
    static
    typename promise<Args...>::future_type
    error(Args2&&... args) {
        promise<Args...> p;
        p.set_exception(std::forward<Args2>(args)...);
        return p.get_future();
    }
};

template<>
struct make_ready_future<void> {
    static
    promise<void>::future_type
    make() {
        promise<void> p;
        p.set_value();
        return p.get_future();
    }

    template<class... Args2>
    static
    typename promise<void>::future_type
    error(Args2&&... args) {
        promise<void> p;
        p.set_exception(std::forward<Args2>(args)...);
        return p.get_future();
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_FUTURE_HPP
