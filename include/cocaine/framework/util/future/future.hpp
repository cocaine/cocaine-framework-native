/*
Copyright (c) 2013 Andrey Goryachev <andrey.goryachev@gmail.com>
Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

This file is part of Cocaine.

Cocaine is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

Cocaine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_FRAMEWORK_FUTURE_IMPL_HPP
#define COCAINE_FRAMEWORK_FUTURE_IMPL_HPP

#include "cocaine/framework/util/future/future_detail.hpp"

namespace cocaine { namespace framework {

template<class... Args>
class future {
    COCAINE_DECLARE_NONCOPYABLE(future)

    typedef typename detail::future::future_traits<Args...>::state_type
            state_type;

    friend future<Args...>
           detail::future::future_from_state<Args...>(state_type&& state, executor_t executor);

    explicit future(state_type&& state,
                    const executor_t& executor) :
        m_state(std::move(state)),
        m_executor(executor)
    {
        // pass
    }

public:
    future()
    {
        // pass
    }

    future(future&& other) :
        m_state(std::move(other.m_state)),
        m_executor(std::move(other.m_executor))
    {
        other.invalidate();
    }

    future&
    operator=(future&& other) {
        m_state = std::move(other.m_state);
        other.invalidate();
        m_executor = std::move(other.m_executor);
        return *this;
    }

    bool
    valid() const {
        return !m_state.empty();
    }

    void
    wait() const {
        check_state();
        m_state.apply(detail::future::wait_visitor<Args...>());
    }

    template<class Rep, class Period>
    void
    wait_for(const std::chrono::duration<Rep, Period>& rel_time) const {
        check_state();
        m_state.apply(detail::future::wait_for_visitor<Rep, Period, Args...>(rel_time));
    }

    template<class Clock, class Duration>
    void
    wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) const {
        check_state();
        m_state.apply(detail::future::wait_until_visitor<Clock, Duration, Args...>(timeout_time));
    }

    bool
    ready() const {
        check_state();
        return m_state.apply(detail::future::ready_visitor<Args...>());
    }

    void
    set_default_executor(const executor_t& executor) {
        m_executor = executor;
    }

    const executor_t&
    get_default_executor() {
        return m_executor;
    }

    typename detail::future::get_visitor<Args...>::result_type
    get() {
        check_state();
        // WARNING: don't write state_type state = std::move(m_state)!
        // G++4.4 doesn't call a move constructor of state_type in such expression.
        state_type state(std::move(m_state));
        invalidate();
        return state.apply(detail::future::get_visitor<Args...>());
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

    // It's similar to 'then', but doesn't invalidate the future. User must store this future until the callback is called.
    template<class F>
    void
    when_ready(executor_t executor,
               F&& callback);

    template<class F>
    void
    when_ready(F&& callback) {
        this->when_ready(this->m_executor, std::forward<F>(callback));
    }

private:
    void
    invalidate() {
        m_state.clean();
    }

    void
    check_state() const {
        if (!valid()) {
            throw future_error(future_errc::no_state);
        }
    }

private:
    state_type m_state;
    executor_t m_executor;
};

template<class... Args>
struct make_ready_future;

template<class... Args>
template<class F>
typename detail::future::unwrapped_result<F, future<Args...>&>::type
future<Args...>::then(executor_t executor,
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

        detail::future::continuation_caller<result_type, future<Args...>> cont(
            std::forward<F>(callback),
            std::move(*this)
        );

        auto task = std::bind(detail::future::task_caller<result_type, future<Args...>&>::call,
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
void
future<Args...>::when_ready(executor_t executor,
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
        m_state.template get<0>()->set_callback(task);
    }
}

template<class... Args>
struct make_ready_future {
    template<class... Args2>
    static
    future<Args...>
    value(Args2&&... args) {
        return detail::future::future_from_state(
            detail::future::ready_state<Args...>(0, std::forward<Args2>(args)...)
        );
    }

    static
    future<Args...>
    error(std::exception_ptr e) {
        return detail::future::future_from_state(detail::future::ready_state<Args...>(e));
    }

    template<class Exception>
    static
    future<Args...>
    error(const Exception& e) {
        return detail::future::future_from_state(
            detail::future::ready_state<Args...>(cocaine::framework::make_exception_ptr(e))
        );
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_FUTURE_IMPL_HPP
