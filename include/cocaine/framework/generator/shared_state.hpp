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

#ifndef COCAINE_FRAMEWORK_GENERATOR_SHARED_STATE_HPP
#define COCAINE_FRAMEWORK_GENERATOR_SHARED_STATE_HPP

#include <cocaine/framework/generator/traits.hpp>
#include <cocaine/framework/future.hpp>

#include <queue>

namespace cocaine { namespace framework { namespace detail { namespace generator {

template<class... Args>
class shared_state {
    typedef typename generator_traits<Args...>::stream_type
            stream_type;

    typedef typename future::storable_type<Args...>::type value_type;
    typedef future::variant<std::exception_ptr, std::system_error> error_type;

    enum {
        exception_tag = 0,
        error_tag
    };

public:
    shared_state() :
        m_closed(false),
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
            try_close();
        }
    }

    bool
    take_future() {
        return m_future_retrieved.exchange(true);
    }

    void
    error(const std::exception_ptr& e) {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        if (!closed()) {
            error_type err;
            err.set<exception_tag>(e);
            set_error(lock, std::move(err));
        } else {
            throw future_error(future_errc::promise_already_satisfied);
        }
    }

    template<class... Args2>
    void
    write(Args2&&... args) {
        std::unique_lock<std::mutex> lock(this->m_access_mutex);
        if (!this->closed()) {
            do_write(lock, std::forward<Args2>(args)...);
        } else {
            throw future_error(future_errc::promise_already_satisfied);
        }
    }

    template<class... Args2>
    void
    try_write(Args2&&... args) {
        std::unique_lock<std::mutex> lock(this->m_access_mutex);
        if (!this->closed()) {
            do_write(lock, std::forward<Args2>(args)...);
        }
    }

    void
    close() {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        if (!closed()) {
            close(lock);
        } else {
            throw future_error(future_errc::promise_already_satisfied);
        }
    }

    void
    try_close() {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        if (!closed()) {
            close(lock);
        }
    }

    bool
    closed() const {
        return m_closed;
    }

    bool
    empty() const {
        return m_values.empty() && m_error.empty();
    }

    bool
    ready() const {
        return closed() || !m_values.empty();
    }

    void
    wait() {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        while (!ready()) {
            m_ready.wait(lock);
        }
    }

    template<class Rep, class Period>
    void
    wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        m_ready.wait_for(lock, rel_time, std::bind(&shared_state::ready, this));
    }

    template<class Clock, class Duration>
    void
    wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        m_ready.wait_until(lock, timeout_time, std::bind(&shared_state::ready, this));
    }

    value_type
    get() {
        while (true) {
            std::unique_lock<std::mutex> lock(m_access_mutex);
            if (!m_values.empty()) {
                value_type result(std::move(m_values.front()));
                m_values.pop();
                return result;
            } else if (m_error.is<exception_tag>()) {
                auto exception = std::move(m_error.get<exception_tag>());
                m_error.clean();
                std::rethrow_exception(exception);
            } else if (m_error.is<error_tag>()) {
                auto error = m_error.get<error_tag>();
                m_error.clean();
                throw error;
            } else if (closed()) {
                throw future_error(future_errc::stream_closed);
            }
            lock.unlock();
            wait();
        }
    }

    void
    redirect(const std::shared_ptr<stream_type>& output) {
        std::unique_lock<std::mutex> lock(m_access_mutex);

        m_callback = std::function<void()>();

        while (!m_values.empty()) {
            output->write(std::move(m_values.front()));
            m_values.pop();
        }

        if (m_error.is<exception_tag>()) {
            output->error(m_error.get<exception_tag>());
        } else if (closed()) {
            output->close();
        } else {
            m_output_stream = output;
        }
    }

    template<class F>
    void
    subscribe(F&& callback) {
        std::unique_lock<std::mutex> lock(m_access_mutex);

        m_output_stream.reset();

        if (!this->m_values.empty() ||
            !this->m_error.empty() ||
            this->closed())
        {
            lock.unlock();
            callback();
        } else {
            m_callback = std::forward<F>(callback);
        }
    }

protected:
    template<class... Args2>
    void
    do_write(std::unique_lock<std::mutex>& lock,
             Args2&&... args)
    {
        if (m_output_stream) {
            m_ready.notify_all();
            m_output_stream->write(value_type(std::forward<Args2>(args)...));
        } else {
            m_values.emplace(std::forward<Args2>(args)...);
            m_ready.notify_all();
            if (m_callback) {
                std::function<void()> callback = m_callback;
                m_callback = std::function<void()>();
                lock.unlock();
                callback();
            }
        }
    }

    template<class... Args2>
    void
    set_error(std::unique_lock<std::mutex>& lock,
              error_type&& e)
    {
        if (m_output_stream) {
            if (e.is<exception_tag>()) {
                m_output_stream->error(e.get<exception_tag>());
            }
        } else {
            m_error = std::move(e);
        }
        close(lock);
    }

    void
    close(std::unique_lock<std::mutex>& lock) {
        m_closed = true;

        m_ready.notify_all();

        std::function<void()> callback = std::move(m_callback);
        auto output = m_output_stream;

        m_callback = std::function<void()>();
        m_output_stream.reset();

        if (callback) {
            lock.unlock();
            callback();
        } else if (output && !output->closed()) {
            lock.unlock();
            output->close();
        }
    }

protected:
    error_type m_error;
    std::queue<value_type> m_values;
    std::atomic<bool> m_closed;

    std::shared_ptr<stream_type> m_output_stream; // stream to redirect to
    std::function<void()> m_callback; // notify once

    std::mutex m_access_mutex;
    std::condition_variable m_ready;

    std::atomic<int> m_promise_counter;
    std::atomic<bool> m_future_retrieved;
};

template<>
class shared_state<void> {
    typedef future::variant<std::exception_ptr, std::system_error> error_type;

    enum {
        exception_tag = 0,
        error_tag
    };

public:
    shared_state() :
        m_closed(false),
        m_void_retrieved(false),
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
            try_close();
        }
    }

    bool
    take_future() {
        return m_future_retrieved.exchange(true);
    }

    void
    error(const std::exception_ptr& e) {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        if (!closed()) {
            error_type err;
            err.set<exception_tag>(e);
            set_error(lock, std::move(err));
        } else {
            throw future_error(future_errc::promise_already_satisfied);
        }
    }

    void
    close() {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        if (!closed()) {
            close(lock);
        } else {
            throw future_error(future_errc::promise_already_satisfied);
        }
    }

    void
    try_close() {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        if (!closed()) {
            close(lock);
        }
    }

    bool
    closed() const {
        return m_closed;
    }

    bool
    empty() const {
        return m_error.empty();
    }

    bool
    ready() const {
        return closed();
    }

    void
    wait() {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        while (!ready()) {
            m_ready.wait(lock);
        }
    }

    template<class Rep, class Period>
    void
    wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        m_ready.wait_for(lock, rel_time, std::bind(&shared_state::ready, this));
    }

    template<class Clock, class Duration>
    void
    wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) {
        std::unique_lock<std::mutex> lock(m_access_mutex);
        m_ready.wait_until(lock, timeout_time, std::bind(&shared_state::ready, this));
    }

    void
    get() {
        while (true) {
            std::unique_lock<std::mutex> lock(m_access_mutex);
            if (m_error.is<exception_tag>()) {
                auto exception = std::move(m_error.get<exception_tag>());
                m_error.clean();
                std::rethrow_exception(exception);
            } else if (closed()) {
                if (!m_void_retrieved) {
                    m_void_retrieved = true;
                    return;
                } else {
                    throw future_error(future_errc::stream_closed);
                }
            }
            lock.unlock();
            wait();
        }
    }

    void
    redirect(const std::shared_ptr<basic_stream<void>>& output) {
        std::unique_lock<std::mutex> lock(m_access_mutex);

        m_callback = std::function<void()>();

        if (m_error.is<exception_tag>()) {
            output->error(m_error.get<exception_tag>());
        } else if (closed()) {
            output->close();
        } else {
            m_output_stream = output;
        }
    }

    template<class F>
    void
    subscribe(F&& callback) {
        std::unique_lock<std::mutex> lock(m_access_mutex);

        m_output_stream.reset();

        if (this->closed())
        {
            lock.unlock();
            callback();
        } else {
            m_callback = std::forward<F>(callback);
        }
    }

protected:
    template<class... Args2>
    void
    set_error(std::unique_lock<std::mutex>& lock,
              error_type&& e)
    {
        if (m_output_stream) {
            if (e.is<exception_tag>()) {
                m_output_stream->error(e.get<exception_tag>());
            }
        } else {
            m_error = std::move(e);
        }
        close(lock);
    }

    void
    close(std::unique_lock<std::mutex>& lock) {
        m_closed = true;

        m_ready.notify_all();

        std::function<void()> callback = std::move(m_callback);
        auto output = m_output_stream;

        m_callback = std::function<void()>();
        m_output_stream.reset();

        if (callback) {
            lock.unlock();
            callback();
        } else if (output && !output->closed()) {
            lock.unlock();
            output->close();
        }
    }

protected:
    error_type m_error;
    std::atomic<bool> m_closed;
    // generator<void>::next must not throw exception when is called for the first time.
    bool m_void_retrieved;

    std::shared_ptr<basic_stream<void>> m_output_stream; // stream to redirect to
    std::function<void()> m_callback; // notify once

    std::mutex m_access_mutex;
    std::condition_variable m_ready;

    std::atomic<int> m_promise_counter;
    std::atomic<bool> m_future_retrieved;
};

}}}} // namespace cocaine::framework::detail::generator

#endif // COCAINE_FRAMEWORK_GENERATOR_SHARED_STATE_HPP
