#ifndef COCAINE_FRAMEWORK_STREAM_HPP
#define COCAINE_FRAMEWORK_STREAM_HPP

#include <cocaine/framework/future.hpp>

#include <queue>

namespace cocaine { namespace framework {

template<class... Args>
struct generator;

template<class... Args>
struct stream;

namespace detail { namespace stream {

    template<class... Args>
    struct when_closed_callback;

    template<class... Args>
    struct shared_stream_state {

        friend class when_closed_callback<Args...>;

        typedef std::tuple<Args...> value_type;

        shared_stream_state() :
            m_is_closed(false)
        {
            // pass
        }

        void
        set_exception(std::exception_ptr e) {
            std::unique_lock<std::mutex> lock(m_ready_mutex);
            if (!closed()) {
                m_exception = e;
                close(lock);
            } else {
                throw future_error(future_errc::promise_already_satisfied);
            }
        }

        void
        try_set_exception(std::exception_ptr e) {
            std::unique_lock<std::mutex> lock(m_ready_mutex);
            if (!closed()) {
                m_exception = e;
                close(lock);
            }
        }

        template<class... Args2>
        void
        push(Args2&&... args) {
            std::unique_lock<std::mutex> lock(this->m_ready_mutex);
            if (!this->closed()) {
                m_result.emplace(std::forward<Args2>(args)...);
                this->make_ready(lock);
            } else {
                throw future_error(future_errc::promise_already_satisfied);
            }
        }

        template<class... Args2>
        void
        try_push(Args2&&... args) {
            std::unique_lock<std::mutex> lock(this->m_ready_mutex);
            if (!this->closed()) {
                m_result.emplace(std::forward<Args2>(args)...);
                this->make_ready(lock);
            }
        }

        void
        close() {
            std::unique_lock<std::mutex> lock(this->m_ready_mutex);
            if (!closed()) {
                close(lock);
            } else {
                throw future_error(future_errc::promise_already_satisfied);
            }
        }

        void
        try_close() {
            std::unique_lock<std::mutex> lock(this->m_ready_mutex);
            if (!closed()) {
                close(lock);
            }
        }

        void
        close(std::unique_lock<std::mutex>& lock) {
            m_is_closed = true;
            if (m_close_callback) {
                m_close_callback();
                m_close_callback = std::function<void()>();
            } else {
                make_ready(lock);
            }
        }

        bool
        closed() const {
            return m_is_closed;
        }

        bool
        ready() const {
            return closed() || !m_result.empty();
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
            m_ready.wait_for(lock, rel_time, std::bind(&shared_stream_state::ready, this));
        }

        template<class Clock, class Duration>
        void
        wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) {
            std::unique_lock<std::mutex> lock(m_ready_mutex);
            m_ready.wait_until(lock, timeout_time, std::bind(&shared_stream_state::ready, this));
        }

        value_type
        get() {
            while (true) {
                if (!m_result.empty()) {
                    value_type result = std::move(m_result.front());
                    m_result.pop();
                    return result;
                } else if (m_exception != std::exception_ptr()) {
                    auto exception = std::move(m_exception);
                    m_exception = std::exception_ptr();
                    std::rethrow_exception(exception);
                } else if (closed()) {
                    throw future_error(future_errc::stream_closed);
                }
                this->wait();
            }
        }

        template<class F>
        void
        set_callback(bool once,
                     F&& callback)
        {
            std::unique_lock<std::mutex> lock(m_ready_mutex);
            m_call_once = once;
            m_callback = callback;
            lock.unlock();
            this->do_calls();
        }

        template<class F>
        void
        on_close(F&& callback)
        {
            std::unique_lock<std::mutex> lock(m_ready_mutex);
            if (this->closed()) {
                lock.unlock();
                callback();
            } else {
                m_close_callback = callback;
            }
        }

    protected:
        void
        make_ready(std::unique_lock<std::mutex>& lock) {
            m_ready.notify_all();
            lock.unlock();
            if (m_callback) {
                this->do_calls();
            }
        }

        void
        do_calls() {
            if (m_call_once) {
                if (m_callback && (!m_result.empty() || m_exception != std::exception_ptr() || closed())) {
                    auto callback = m_callback;
                    m_callback = std::function<void()>();
                    callback();
                }
            } else if (m_callback) {
                while (!m_result.empty()) {
                    m_callback();
                }

                if (m_exception != std::exception_ptr() || closed()) {
                    m_callback();
                    m_callback = std::function<void()>();
                }
            }
        }

    protected:
        std::exception_ptr m_exception;
        std::queue<value_type> m_result;
        std::function<void()> m_callback;
        bool m_call_once; // call m_callback once or on each item
        std::function<void()> m_close_callback;
        std::atomic<bool> m_is_closed;
        std::mutex m_ready_mutex;
        std::condition_variable m_ready;
    };

    // create generator from shared_stream_state
    template<class... Args>
    inline
    cocaine::framework::generator<Args...>
    generator_from_state(std::shared_ptr<shared_stream_state<Args...>>& state,
                      executor_t executor = executor_t())
    {
        return cocaine::framework::generator<Args...>(state, executor);
    }

    template<class... Args>
    struct storable_type {
        typedef std::tuple<Args...> type;

        static
        type
        pop_from(std::queue<std::tuple<Args...>>& q) {
            type result = std::move(q.front());
            q.pop();
            return result;
        }
    };

    template<class T>
    struct storable_type<T> {
        typedef T type;

        static
        type
        pop_from(std::queue<std::tuple<T>>& q) {
            type result = std::move(std::get<0>(q.front()));
            q.pop();
            return result;
        }
    };

    template<class T>
    struct storable_type<T&> {
        typedef std::reference_wrapper<T> type;

        static
        type
        pop_from(std::queue<std::tuple<T&>>& q) {
            type result(std::get<0>(q.front()));
            q.pop();
            return result;
        }
    };

    template<class... Args>
    struct when_closed_callback {
        typedef promise<std::vector<typename storable_type<Args...>::type>>
                promise_type;

        when_closed_callback(std::shared_ptr<shared_stream_state<Args...>> state) :
            m_state(state)
        {
            m_promise.reset(new promise_type());
        }

        void
        operator()() {
            if (m_state->m_exception) {
                m_promise->set_exception(m_state->m_exception);
            } else {
                std::vector<typename storable_type<Args...>::type> result;
                while (!m_state->m_result.empty()) {
                    result.emlace(storable_type<Args...>::pop_from(m_state->m_result));
                }
                m_promise->set_value(std::move(result));
            }
        }

        typename promise_type::future_type
        get_future() const {
            return m_promise->get_future();
        }

    private:
        std::shared_ptr<shared_stream_state<Args...>> m_state;
        std::shared_ptr<promise_type> m_promise;
    };

    template<class Result, class... Args>
    struct map_callback {
        typedef cocaine::framework::generator<Args...>
                producer_type;

        typedef cocaine::framework::future<Args...>
                future_type;

        typedef cocaine::framework::packaged_task<Result(future_type&)>
                task_type;

        typedef decltype(task_type().get_future())
                result_type;

        map_callback(task_type&& task,
                     producer_type&& producer) :
            m_callback(std::move(task)),
            m_producer(std::move(producer))
        {
            // pass
        }

        map_callback(map_callback&& other) :
            m_callback(std::move(other.m_callback)),
            m_producer(std::move(other.m_producer)),
            m_consumer(std::move(other.m_consumer))
        {
            // pass
        }

        cocaine::framework::generator<result_type>
        get_generator() {
            return m_consumer.get_generator();
        }

        void
        operator()() {
            m_callback.reset();
            future_type f;
            try {
                f = make_ready_future<Args...>::make(m_producer.get());
            } catch (const future_error& e) {
                f = make_ready_future<Args...>::error(std::current_exception());
                if (e.code() == future_errc::stream_closed) {
                    m_callback(std::move(f));
                    return;
                }
            } catch (...) {
                f = make_ready_future<Args...>::error(std::current_exception());
            }
            m_callback(std::move(f));
            m_consumer.push_value(m_callback.get_future());
        }

    private:
        task_type m_callback;
        producer_type m_producer;
        cocaine::framework::stream<result_type> m_consumer;
    };

}} // namespace detail::stream

template<class... Args>
struct generator {
    COCAINE_DECLARE_NONCOPYABLE(generator)

    friend generator<Args...>
           detail::stream::generator_from_state<Args...>(
               std::shared_ptr<shared_stream_state<Args...>>& state,
               executor_t executor
           );

    generator() {
        // pass
    }

    generator(generator&& other) :
        m_state(std::move(other.m_state)),
        m_executor(std::move(other.m_executor))
    {
        // pass
    }

    void
    operator=(generator&& other) {
        m_state = std::move(other.m_state);
        m_executor = std::move(other.m_executor);
    }

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

    typename detail::future::getter<Args...>::result_type
    get() {
        check_state();
        return detail::future::getter<Args...>::get(*m_state);
    }

    bool
    ready() const {
        check_state();
        return m_state->ready();
    }

    bool
    closed() const {
        check_state();
        return m_state->closed();
    }

    void
    set_default_executor(const executor_t& executor) {
        m_executor = executor;
    }

    // calls callback with the generator when next item will be ready
    // calls callback only once (for first available item)
    // invalidates current generator
    template<class F>
    typename detail::future::unwrapped_result<F, generator<Args...>>::type
    next(executor_t executor,
         F&& callback);

    template<class F>
    typename detail::future::unwrapped_result<F, generator<Args...>>::type
    next(F&& callback) {
        return next(m_executor, std::forward<F>(callback));
    }

    // calls callback for each item or exception
    // returns new generator of futures - results of callbacks
    template<class F>
    generator<typename detail::future::unwrapped_result<F, generator<Args...>>::type>
    map(executor_t executor,
        F&& callback);

    template<class F>
    generator<typename detail::future::unwrapped_result<F, generator<Args...>>::type>
    map(F&& callback) {
        return map(m_executor, std::forward<F>(callback));
    }

    // returns future that becomes ready when stream is closed
    // invalidates generator
    // if Args... is T& then result is future<vector<reference_wrapper<T>>>
    future<std::vector<typename detail::stream::storable_type<Args...>::type>>
    when_closed() {
        check_state();

        detail::stream::when_closed_callback<Args...> callback(m_state);
        m_state->on_close(callback);
        invalidate();

        return callback.get_future();
    }

protected:
    typedef std::shared_ptr<detail::stream::shared_stream_state<Args...>> state_type;

    explicit generator(state_type state,
                       executor_t executor):
        m_state(state),
        m_executor(executor)
    {
        // pass
    }

    void
    check_state() const {
        if (!valid()) {
            throw future_error(future_errc::no_state);
        }
    }

    void
    invalidate() {
        m_state.reset();
    }

protected:
    state_type m_state;
    executor_t m_executor;
};

template<class... Args>
template<class F>
typename detail::future::unwrapped_result<F, generator<Args...>>::type
generator<Args...>::next(executor_t executor,
                         F&& callback)
{
    this->check_state();

    typedef decltype(callback(*((generator*)(nullptr)))) result_type;

    auto old_state = this->m_state;

    auto task = std::make_shared<packaged_task<result_type()>>(
        executor,
        detail::future::continuation_caller<result_type, generator<Args...>>(
            std::forward<F>(callback),
            std::move(*this)
        )
    );

    auto result = task->get_future();

    old_state->set_callback(
        true,
        detail::future::shared_callable<packaged_task<result_type()>>(task)
    );

    return result;
}

template<class... Args>
template<class F>
generator<typename detail::future::unwrapped_result<F, generator<Args...>>::type>
generator<Args...>::map(executor_t executor,
                        F&& callback)
{
    this->check_state();

    auto old_state = this->m_state;

    typedef decltype(callback(*((generator*)(nullptr)))) result_type;

    auto task = std::make_shared<detail::stream::map_callback<result_type, Args...>>(
        packaged_task<result_type(future<Args...>&)>(executor, std::forward<F>(callback)),
        std::move(*this)
    );

    old_state->set_callback(
        false,
        detail::future::shared_callable<detail::stream::map_callback<result_type, Args...>>(task)
    );

    return task->get_generator();
}

template<class... Args>
struct stream {
    COCAINE_DECLARE_NONCOPYABLE(stream)

    stream() :
        m_state(new detail::stream::shared_stream_state<Args...>()),
        m_retrieved(false)
    {
        // pass
    }

    stream(stream&& other) :
        m_state(std::move(other.m_state)),
        m_retrieved(other.m_retrieved)
    {
        // pass
    }

    ~stream() {
        if (m_state) {
            m_state->try_close();
        }
    }

    void
    operator=(stream&& other) {
        m_state = std::move(other.m_state);
        m_retrieved = other.m_retrieved;
    }

    template<class... Args2>
    void
    push_value(Args2&&... args) {
        if (m_state) {
            m_state->push(std::forward<Args2>(args)...);
        } else {
            throw future_error(future_errc::no_state);
        }
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
                detail::future::make_exception_ptr(e)
            );
        } else {
            throw future_error(future_errc::no_state);
        }
    }

    void
    close() {
        if (m_state) {
            m_state->close();
        }
    }

    cocaine::framework::generator<Args...>
    get_generator() {
        if (!m_state) {
            throw future_error(future_errc::no_state);
        } else if (m_retrieved) {
            throw future_error(future_errc::future_already_retrieved);
        } else {
            m_retrieved = true;
            return detail::stream::generator_from_state<Args...>(m_state);
        }
    }

protected:
    std::shared_ptr<detail::stream::shared_stream_state<Args...>> m_state;
    bool m_retrieved;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_STREAM_HPP
