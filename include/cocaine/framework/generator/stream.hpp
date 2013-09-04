#ifndef COCAINE_FRAMEWORK_GENERATOR_STREAM_HPP
#define COCAINE_FRAMEWORK_GENERATOR_STREAM_HPP

#include <cocaine/framework/generator/generator.hpp>

namespace cocaine { namespace framework {

namespace detail { namespace generator {

    template<class... Args>
    class state_handler {
        COCAINE_DECLARE_NONCOPYABLE(state_handler)
    public:
        state_handler() :
            m_state(new shared_state<Args...>()),
            m_generator_retrieved(false)
        {
            // pass
        }

        ~state_handler() {
            m_state->try_close();
        }

        cocaine::framework::generator<Args...>
        get_generator() {
            bool retrieved = m_generator_retrieved.exchange(true);
            if (!retrieved) {
                return generator_from_state<Args...>(m_state);
            } else {
                throw future_error(future_errc::future_already_retrieved);
            }
        }

        std::shared_ptr<shared_state<Args...>>
        state() const {
            return m_state;
        }

    protected:
        std::shared_ptr<shared_state<Args...>> m_state;
        std::atomic<bool> m_generator_retrieved;
    };

}} // namespace detail::generator

template<class... Args>
class stream :
    public generator_traits<Args...>::stream_type
{
public:
    stream() :
        m_state(new detail::generator::state_handler<Args...>())
    {
        // pass
    }

    void
    write(typename generator_traits<Args...>::single_type&& value) {
        state()->write(std::move(value));
    }

    template<class... Args2>
    void
    write(Args2&&... args) {
        state()->write(std::forward<Args2>(args)...);
    }

    void
    error(const std::exception_ptr& e) {
        state()->error(e);
    }

    void
    error(const std::system_error& e) {
        state()->error(e);
    }

    void
    close() {
        state()->close();
    }

    bool
    closed() const {
        return state()->closed();
    }

    generator<Args...>
    get_generator() {
        check_state();
        return m_state->get_generator();
    }

protected:
    std::shared_ptr<detail::generator::shared_state<Args...>>
    state() const {
        check_state();
        return m_state->state();
    }

    void
    check_state() const {
        if (!m_state) {
            throw future_error(future_errc::no_state);
        }
    }

private:
    std::shared_ptr<detail::generator::state_handler<Args...>> m_state;
};

template<>
class stream<void> :
    public generator_traits<void>::stream_type
{
public:
    stream() :
        m_state(new detail::generator::state_handler<void>())
    {
        // pass
    }

    void
    error(const std::exception_ptr& e) {
        state()->error(e);
    }

    void
    error(const std::system_error& e) {
        state()->error(e);
    }

    void
    close() {
        state()->close();
    }

    bool
    closed() const {
        return state()->closed();
    }

    generator<void>
    get_generator() {
        check_state();
        return m_state->get_generator();
    }

protected:
    std::shared_ptr<detail::generator::shared_state<void>>
    state() const {
        check_state();
        return m_state->state();
    }

    void
    check_state() const {
        if (!m_state) {
            throw future_error(future_errc::no_state);
        }
    }

private:
    std::shared_ptr<detail::generator::state_handler<void>> m_state;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_GENERATOR_STREAM_HPP
