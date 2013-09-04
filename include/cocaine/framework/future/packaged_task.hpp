#ifndef COCAINE_FRAMEWORK_FUTURE_PACKAGED_TASK_HPP
#define COCAINE_FRAMEWORK_FUTURE_PACKAGED_TASK_HPP

#include <cocaine/framework/future/basic_promise.hpp>

namespace cocaine { namespace framework {

template<class T>
class packaged_task;

template<class R, class... Args>
class packaged_task<R(Args...)> :
    public detail::future::basic_promise<R>
{
public:
    packaged_task() :
        detail::future::basic_promise<R>(std::shared_ptr<detail::future::shared_state<R>>())
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
        detail::future::basic_promise<R>(std::move(other)),
        m_function(std::move(other.m_function)),
        m_executor(std::move(other.m_executor))
    {
        // pass
    }

    packaged_task&
    operator=(packaged_task&& other) {
        m_function = std::move(other.m_function);
        m_executor = std::move(other.m_executor);
        detail::future::basic_promise<R>::operator=(std::move(other));
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
        auto f = detail::future::basic_promise<R>::get_future().unwrap();
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
    using detail::future::basic_promise<R>::set_exception;

    std::function<R(Args...)> m_function;
    executor_t m_executor;
};

}} // namespace cocaine::framework

#endif //COCAINE_FRAMEWORK_FUTURE_PACKAGED_TASK_HPP
