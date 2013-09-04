#ifndef COCAINE_FRAMEWORK_FUTURE_PACKAGED_TASK_HPP
#define COCAINE_FRAMEWORK_FUTURE_PACKAGED_TASK_HPP

#include <cocaine/framework/future/promise.hpp>

namespace cocaine { namespace framework {

template<class T>
class packaged_task;

template<class R, class... Args>
class packaged_task<R(Args...)> :
    public promise<R>
{
public:
    packaged_task() :
        promise<R>(std::shared_ptr<detail::future::state_handler<R>>())
    {
        // pass
    }

    explicit
    packaged_task(const std::function<R(Args...)>& f) :
        m_function(f)
    {
        // pass
    }

    packaged_task(const executor_t& executor,
                  const std::function<R(Args...)>& f) :
        m_function(f),
        m_executor(executor)
    {
        // pass
    }

    bool
    valid() const {
        return static_cast<bool>(m_function);
    }

    void
    reset() {
        *this = packaged_task<R(Args...)>();
    }

    typename detail::future::unwrapper<future<R>>::unwrapped_type
    get_future() {
        auto f = promise<R>::get_future().unwrap();
        f.set_default_executor(m_executor);
        return f;
    }

    template<class... Args2>
    void
    operator()(Args2&&... args);

private:
    using promise<R>::set_exception;
    using promise<R>::set_value;

    std::function<R(Args...)> m_function;
    executor_t m_executor;
};

template<class R, class... Args>
template<class... Args2>
void
packaged_task<R(Args...)>::operator()(Args2&&... args) {
    if (m_executor) {
        m_executor(std::bind(detail::future::task_caller<R, Args...>::call,
                             this->state(),
                             m_function,
                             std::forward<Args2>(args)...));
    } else {
        detail::future::task_caller<R, Args...>::call(this->state(),
                                                      m_function,
                                                      std::forward<Args2>(args)...);
    }
}

}} // namespace cocaine::framework

#endif //COCAINE_FRAMEWORK_FUTURE_PACKAGED_TASK_HPP
