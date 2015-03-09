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

#ifndef COCAINE_FRAMEWORK_GENERATOR_STREAM_HPP
#define COCAINE_FRAMEWORK_GENERATOR_STREAM_HPP

#include <cocaine/framework/generator/generator.hpp>

namespace cocaine { namespace framework {

template<class... Args>
class stream :
    public generator_traits<Args...>::stream_type
{
public:
    void
    write(typename generator_traits<Args...>::single_type&& value) {
        m_state.state()->write(std::move(value));
    }

    template<class... Args2>
    void
    write(Args2&&... args) {
        m_state.state()->write(std::forward<Args2>(args)...);
    }

    void
    error(const std::exception_ptr& e) {
        m_state.state()->error(e);
    }

    void
    close() {
        m_state.state()->close();
    }

    bool
    closed() const {
        return m_state.state()->closed();
    }

    generator<Args...>
    get_generator() {
        if (!m_state.state()->take_future()) {
            return detail::generator::generator_from_state<Args...>(m_state.state());
        } else {
            throw future_error(future_errc::future_already_retrieved);
        }
    }

private:
    typedef detail::generator::shared_state<Args...>
            state_type;

    detail::future::state_handler<state_type> m_state;
};

template<>
class stream<void> :
    public generator_traits<void>::stream_type
{
public:
    void
    error(const std::exception_ptr& e) {
        m_state.state()->error(e);
    }

    void
    close() {
        m_state.state()->close();
    }

    bool
    closed() const {
        return m_state.state()->closed();
    }

    generator<void>
    get_generator() {
        if (!m_state.state()->take_future()) {
            return detail::generator::generator_from_state<void>(m_state.state());
        } else {
            throw future_error(future_errc::future_already_retrieved);
        }
    }

private:
    typedef detail::generator::shared_state<void>
            state_type;

    detail::future::state_handler<state_type> m_state;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_GENERATOR_STREAM_HPP
