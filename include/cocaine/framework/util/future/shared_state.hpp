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

#ifndef COCAINE_FRAMEWORK_FUTURE_SHARED_STATE_HPP
#define COCAINE_FRAMEWORK_FUTURE_SHARED_STATE_HPP

#include <cocaine/framework/util/future/variant.hpp>
#include <cocaine/framework/util/future/error.hpp>
#include <cocaine/framework/util/future/traits.hpp>

#include <cocaine/framework/common.hpp>

#include <ios>
#include <string>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <exception>

namespace cocaine { namespace framework { namespace detail { namespace future {

// shared state of promise-future
// it's a "core" of futures, while "future" and "promise" are just wrappers to access shared state
template<class... Args>
class shared_state {
    COCAINE_DECLARE_NONCOPYABLE(shared_state)

    typedef typename future_traits<Args...>::storable_type value_type;

    typedef variant<value_type, std::exception_ptr> result_type;

    enum result_tag {
        value_tag,
        exception_tag
    };

public:
    shared_state() :
        m_promise_counter(0),
        m_future_retrieved(false)
    {
        // pass
    }

    void
    new_promise() {
        ++m_promise_counter;
    }

    void
    release_promise() {
        auto counter = --m_promise_counter;
        if (counter == 0) {
            try_set_exception(
                cocaine::framework::make_exception_ptr(future_error(future_errc::broken_promise))
            );
        }
    }

    bool
    take_future() {
        return m_future_retrieved.exchange(true);
    }

    void
    set_exception(std::exception_ptr e) {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        if (!ready(lock)) {
            m_result.template set<exception_tag>(e);
            make_ready(lock);
        } else {
            throw future_error(future_errc::promise_already_satisfied);
        }
    }

    void
    try_set_exception(std::exception_ptr e) {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        if (!ready(lock)) {
            m_result.template set<exception_tag>(e);
            make_ready(lock);
        }
    }

    template<class... Args2>
    void
    set_value(Args2&&... args) {
        std::unique_lock<std::mutex> lock(this->m_access_mutex);
        if (!this->ready(lock)) {
            this->m_result.template set<value_tag>(std::forward<Args2>(args)...);
            this->make_ready(lock);
        } else {
            throw future_error(future_errc::promise_already_satisfied);
        }
    }

    template<class... Args2>
    void
    try_set_value(Args2&&... args) {
        std::unique_lock<std::mutex> lock(this->m_access_mutex);
        if (!this->ready(lock)) {
            this->m_result.template set<value_tag>(std::forward<Args2>(args)...);
            this->make_ready(lock);
        }
    }

    value_type&
    get() {
        wait();

        if (m_result.template is<exception_tag>()) {
            std::rethrow_exception(m_result.template get<exception_tag>());
        }

        return m_result.template get<value_tag>();
    }

    void
    wait() {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        while (!ready(lock)) {
            m_ready.wait(lock);
        }
    }

    template<class Rep, class Period>
    void
    wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        m_ready.wait_for(lock, rel_time, [this, &lock]() { return ready(lock); });
    }

    template<class Clock, class Duration>
    void
    wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        m_ready.wait_until(lock, timeout_time, [this, &lock]() { return ready(lock); });
    }

    bool
    ready() const {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        return !m_result.empty();
    }

    bool
    ready(std::unique_lock<std::mutex>&) const {
        return !m_result.empty();
    }

    template<class F>
    void
    set_callback(F&& callback) {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        if (this->ready(lock)) {
            lock.unlock();
            callback();
        } else {
            m_callback = std::forward<F>(callback);
        }
    }

private:
    void
    make_ready(std::unique_lock<std::mutex>& lock) {
        m_ready.notify_all();
        lock.unlock();
        if (m_callback) {
            m_callback();
            m_callback = std::function<void()>();
        }
    }

private:
    result_type m_result;

    std::function<void()> m_callback;

    mutable std::mutex m_access_mutex;
    std::condition_variable m_ready;

    std::atomic<int> m_promise_counter;
    std::atomic<bool> m_future_retrieved;
};

template<class... Args>
class ready_state {
    COCAINE_DECLARE_NONCOPYABLE(ready_state)

    typedef typename future_traits<Args...>::storable_type value_type;

    typedef variant<value_type, std::exception_ptr> result_type;

    static const unsigned int value_tag = 0;
    static const unsigned int exception_tag = 1;

public:
    ready_state(std::exception_ptr e) {
        m_result.template set<exception_tag>(e);
    }

    template<class... Args2>
    ready_state(int, Args2&&... args) { // first int is to construct ready future<std::exception_ptr>
        m_result.template set<value_tag>(std::forward<Args2>(args)...);
    }

    ready_state(ready_state&& other) :
        m_result(std::move(other.m_result))
    {
        // pass
    }

    value_type&
    get() {
        if (m_result.template is<exception_tag>()) {
            std::rethrow_exception(m_result.template get<exception_tag>());
        }

        return m_result.template get<value_tag>();
    }

    void
    wait() const {
        // pass
    }

    template<class Rep, class Period>
    void
    wait_for(const std::chrono::duration<Rep, Period>& /*rel_time*/) const {
        // pass
    }

    template<class Clock, class Duration>
    void
    wait_until(const std::chrono::time_point<Clock, Duration>& /*timeout_time*/) const {
        // pass
    }

    bool
    ready() const {
        return true;
    }

    template<class F>
    void
    set_callback(F&& callback) {
        callback();
    }

private:
    result_type m_result;
};

}}}} // namespace cocaine::framework::detail::future

#endif // COCAINE_FRAMEWORK_FUTURE_SHARED_STATE_HPP
