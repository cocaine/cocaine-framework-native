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
