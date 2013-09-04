#ifndef COCAINE_FRAMEWORK_GENERATOR_STREAM_HPP
#define COCAINE_FRAMEWORK_GENERATOR_STREAM_HPP

#include <cocaine/framework/generator/generator.hpp>

namespace cocaine { namespace framework {

template<class... Args>
class stream :
    public generator_traits<Args...>::stream_type
{
public:
    stream() :
        m_state(new detail::generator::shared_state<Args...>())
    {
        // pass
    }

    stream(stream&& other) :
        m_state(std::move(other.m_state))
    {
        // pass
    }

    ~stream() {
        if (m_state) {
            m_state->try_close();
        }
    }

    void
    operator=(stream&& other) {
        m_state = std::move(other.m_state);
    }

    void
    write(typename generator_traits<Args...>::single_type&& value) {
        if (m_state) {
            m_state->write(std::move(value));
        } else {
            throw future_error(future_errc::no_state);
        }
    }

    template<class... Args2>
    void
    write(Args2&&... args) {
        if (m_state) {
            m_state->write(std::forward<Args2>(args)...);
        } else {
            throw future_error(future_errc::no_state);
        }
    }

    void
    error(const std::exception_ptr& e) {
        if (m_state) {
            m_state->error(e);
        } else {
            throw future_error(future_errc::no_state);
        }
    }

    void
    error(const std::system_error& e) {
        if (m_state) {
            m_state->error(e);
        } else {
            throw future_error(future_errc::no_state);
        }
    }

    void
    close() {
        if (m_state) {
            m_state->close();
        }
    }

    bool
    closed() const {
        if (m_state) {
            return m_state->closed();
        } else {
            return true;
        }
    }

    generator<Args...>
    get_generator() {
        if (m_state) {
            bool retrieved = m_state->check_in();
            if (!retrieved) {
                return detail::generator::generator_from_state<Args...>(m_state);
            } else {
                throw future_error(future_errc::future_already_retrieved);
            }
        } else {
            throw future_error(future_errc::no_state);
        }
    }

protected:
    std::shared_ptr<detail::generator::shared_state<Args...>> m_state;
};

template<>
class stream<void> :
    public generator_traits<void>::stream_type
{
public:
    stream() :
        m_state(new detail::generator::shared_state<void>())
    {
        // pass
    }

    stream(stream&& other) :
        m_state(std::move(other.m_state))
    {
        // pass
    }

    ~stream() {
        if (m_state) {
            m_state->try_close();
        }
    }

    void
    operator=(stream&& other) {
        m_state = std::move(other.m_state);
    }

    void
    error(const std::exception_ptr& e) {
        if (m_state) {
            m_state->error(e);
        } else {
            throw future_error(future_errc::no_state);
        }
    }

    void
    error(const std::system_error& e) {
        if (m_state) {
            m_state->error(e);
        } else {
            throw future_error(future_errc::no_state);
        }
    }

    void
    close() {
        if (m_state) {
            m_state->close();
        }
    }

    bool
    closed() const {
        if (m_state) {
            return m_state->closed();
        } else {
            return true;
        }
    }

    generator<void>
    get_generator() {
        if (m_state) {
            bool retrieved = m_state->check_in();
            if (!retrieved) {
                return detail::generator::generator_from_state<void>(m_state);
            } else {
                throw future_error(future_errc::future_already_retrieved);
            }
        } else {
            throw future_error(future_errc::no_state);
        }
    }

protected:
    std::shared_ptr<detail::generator::shared_state<void>> m_state;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_GENERATOR_STREAM_HPP
