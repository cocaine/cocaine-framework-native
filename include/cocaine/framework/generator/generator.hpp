#ifndef COCAINE_FRAMEWORK_GENERATOR_IMPL_HPP
#define COCAINE_FRAMEWORK_GENERATOR_IMPL_HPP

#include <cocaine/framework/generator/generator_detail.hpp>

namespace cocaine { namespace framework {

template<class... Args>
class generator {
    COCAINE_DECLARE_NONCOPYABLE(generator)

    typedef std::shared_ptr<detail::generator::shared_state<Args...>> state_type;

    friend generator<Args...>
           detail::generator::generator_from_state<Args...>(const state_type& state,
                                                            executor_t executor);

    explicit generator(state_type state,
                       const executor_t& executor):
        m_state(state),
        m_executor(executor)
    {
        // pass
    }

public:
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
        m_state->wait_for(rel_time);
    }

    template<class Clock, class Duration>
    void
    wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) const {
        check_state();
        m_state->wait_until(timeout_time);
    }

    typename detail::future::getter<Args...>::result_type
    next() {
        check_state();
        return detail::future::getter<Args...>::get(*m_state);
    }

    bool
    ready() const {
        check_state();
        return m_state->ready();
    }

    bool
    empty() const {
        check_state();
        return m_state->empty();
    }

    bool
    closed() const {
        check_state();
        return m_state->empty() && m_state->closed();
    }

    void
    set_default_executor(const executor_t& executor) {
        m_executor = executor;
    }

    const executor_t&
    get_default_executor() {
        return m_executor;
    }

    // Redirects output of the generator into stream. Invalidates this generator.
    void
    redirect(const std::shared_ptr<typename generator_traits<Args...>::stream_type>& output);

    // It's similar to 'then', but doesn't invalidate the generator. User must store this generator until the callback is called.
    template<class F>
    void
    when_ready(executor_t executor,
               F&& callback);

    template<class F>
    void
    when_ready(F&& callback) {
        this->when_ready(this->m_executor, std::forward<F>(callback));
    }

    // Calls callback with the generator when next item will be ready. Invalidates this generator.
    // It calls callback only once, for first available item.
    template<class F>
    typename detail::future::unwrapped_result<F, generator<Args...>&>::type
    then(executor_t executor,
         F&& callback);

    template<class F>
    typename detail::future::unwrapped_result<F, generator<Args...>&>::type
    then(F&& callback) {
        return then(m_executor, std::forward<F>(callback));
    }

    // Calls callback for each item or exception.
    // Returns new generator of futures - results of callback.
    template<class F>
    generator<typename detail::generator::unwrapped_result<F, future<Args...>>::type>
    map(executor_t executor,
        F&& callback);

    template<class F>
    generator<typename detail::generator::unwrapped_result<F, future<Args...>>::type>
    map(F&& callback) {
        return map(m_executor, std::forward<F>(callback));
    }

    // Returns future that becomes ready when the stream is closed. Invalidates this generator.
    // If Args... is T& then result is future<vector<reference_wrapper<T>>>.
    // If Args... is void then result is future<void>.
    future<typename detail::generator::gather_stream<Args...>::result_type>
    gather();

protected:
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
void
generator<Args...>::redirect(
    const std::shared_ptr<typename generator_traits<Args...>::stream_type>& output
)
{
    check_state();

    m_state->redirect(output);
    m_state.reset();
}

template<class... Args>
template<class F>
void
generator<Args...>::when_ready(executor_t executor,
                               F&& callback)
{
    this->check_state();

    if (this->ready()) {
        if (!executor) {
            callback(*this);
        } else {
            executor(std::bind(std::forward<F>(callback), std::ref(*this)));
        }
    } else {
        std::function<void()> task = std::bind(std::forward<F>(callback), std::ref(*this));
        if (executor) {
            task = std::bind(executor, task);
        }
        m_state->subscribe(task);
    }
}

template<class... Args>
template<class F>
typename detail::future::unwrapped_result<F, generator<Args...>&>::type
generator<Args...>::then(executor_t executor,
                         F&& callback)
{
    this->check_state();

    typedef decltype(callback(*this)) result_type;

    future<result_type> result;

    if (!executor && ready()) { // Here we can call callback right now. Just a little optimization.
        try {
            result = detail::future::ready_from_task(std::forward<F>(callback), *this);
        } catch (...) {
            result = make_ready_future<result_type>::error(std::current_exception());
        }
        this->invalidate();
    } else {
        auto new_state = std::make_shared<detail::future::shared_state<result_type>>();

        detail::future::continuation_caller<result_type, generator<Args...>> cont(
            std::forward<F>(callback),
            std::move(*this)
        );

        auto task = std::bind(detail::future::task_caller<result_type, generator<Args...>&>::call,
                              new_state,
                              cont,
                              std::placeholders::_1);

        cont.get_future().when_ready(task);

        result = detail::future::future_from_state<result_type>(new_state);
    }

    return result.unwrap();
}

template<class... Args>
template<class F>
generator<typename detail::generator::unwrapped_result<F, future<Args...>>::type>
generator<Args...>::map(executor_t executor,
                        F&& callback)
{
    this->check_state();

    typedef decltype(future<Args...>().unwrap()) fut_type;
    typedef decltype(callback(declval<fut_type&>())) result_type;

    auto output = std::make_shared<detail::generator::map_stream<result_type, Args...>>(
        executor,
        std::forward<F>(callback)
    );

    this->redirect(output);

    return output->get_generator();
}

template<class... Args>
future<typename detail::generator::gather_stream<Args...>::result_type>
generator<Args...>::gather() {
    check_state();
    auto output = std::make_shared<detail::generator::gather_stream<Args...>>();
    redirect(output);
    return output->get_future();
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_GENERATOR_IMPL_HPP
