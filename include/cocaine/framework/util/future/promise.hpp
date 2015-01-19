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

#ifndef COCAINE_FRAMEWORK_FUTURE_PROMISE_HPP
#define COCAINE_FRAMEWORK_FUTURE_PROMISE_HPP

#include <cocaine/framework/util/future/future.hpp>

namespace cocaine { namespace framework {

namespace detail { namespace future {

    template<class State>
    class state_handler {
    public:
        state_handler(const std::shared_ptr<State>& state = std::make_shared<State>()) :
            m_state(state)
        {
            if (m_state) {
                m_state->new_promise();
            }
        }

        state_handler(const state_handler& other) :
            m_state(other.m_state)
        {
            if (m_state) {
                m_state->new_promise();
            }
        }

        state_handler(state_handler&& other) :
            m_state(std::move(other.m_state))
        {
            // pass
        }

        ~state_handler() {
            if (m_state) {
                m_state->release_promise();
            }
        }

        state_handler&
        operator=(const state_handler& other) {
            if (m_state) {
                m_state->release_promise();
            }
            m_state = other.m_state;
            if (m_state) {
                m_state->new_promise();
            }
            return *this;
        }

        state_handler&
        operator=(state_handler&& other) {
            if (m_state) {
                m_state->release_promise();
            }
            m_state = std::move(other.m_state);
            return *this;
        }

        const std::shared_ptr<State>&
        state() const {
            if (!m_state) {
                throw future_error(future_errc::no_state);
            } else {
                return m_state;
            }
        }

    protected:
        std::shared_ptr<State> m_state;
    };

}} // namespace detail::future

template<class... Args>
class promise {
public:
    typedef future<Args...>
            future_type;

    promise() {
        // pass
    }

    void
    set_exception(std::exception_ptr e) {
        m_state.state()->set_exception(e);
    }

    template<class Exception>
    void
    set_exception(const Exception& e) {
        m_state.state()->set_exception(
            cocaine::framework::make_exception_ptr(e)
        );
    }

    template<class... Args2>
    void
    set_value(Args2&&... args) {
        m_state.state()->set_value(std::forward<Args2>(args)...);
    }

    future<Args...>
    get_future() {
        if (!m_state.state()->take_future()) {
            return detail::future::future_from_state<Args...>(m_state.state());
        } else {
            throw future_error(future_errc::future_already_retrieved);
        }
    }

protected:
    typedef detail::future::shared_state<Args...>
            state_type;

    explicit
    promise(const std::shared_ptr<state_type>& state) :
        m_state(state)
    {
        // pass
    }

    const std::shared_ptr<state_type>&
    state() const {
        return m_state.state();
    }

private:
    detail::future::state_handler<state_type> m_state;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_FUTURE_PROMISE_HPP
