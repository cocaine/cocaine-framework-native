#ifndef COCAINE_FRAMEWORK_FUTURE_BASIC_PROMISE_HPP
#define COCAINE_FRAMEWORK_FUTURE_BASIC_PROMISE_HPP

#include <cocaine/framework/future/future.hpp>

namespace cocaine { namespace framework { namespace detail { namespace future {

template<class... Args>
class basic_promise {
public:
    basic_promise() :
        m_state(new shared_state<Args...>())
    {
        // pass
    }

    basic_promise(basic_promise&& other) :
        m_state(std::move(other.m_state))
    {
        // pass
    }

    ~basic_promise() {
        if (m_state) {
            m_state->try_set_exception(
                cocaine::framework::make_exception_ptr(future_error(future_errc::broken_promise))
            );
        }
    }

    void
    operator=(basic_promise&& other) {
        m_state = std::move(other.m_state);
    }

    void
    set_exception(std::exception_ptr e) {
        if (m_state) {
            m_state->set_exception(e);
        } else {
            throw future_error(future_errc::no_state);
        }
    }

    template<class Exception>
    void
    set_exception(const Exception& e) {
        if (m_state) {
            m_state->set_exception(
                cocaine::framework::make_exception_ptr(e)
            );
        } else {
            throw future_error(future_errc::no_state);
        }
    }

    cocaine::framework::future<Args...>
    get_future() {
        if (m_state) {
            bool retrieved = m_state->check_in();
            if (!retrieved) {
                return future_from_state<Args...>(m_state);
            } else {
                throw future_error(future_errc::future_already_retrieved);
            }
        } else {
            throw future_error(future_errc::no_state);
        }
    }

protected:
    explicit
    basic_promise(const std::shared_ptr<shared_state<Args...>>& state) :
        m_state(state)
    {
        // pass
    }

    std::shared_ptr<shared_state<Args...>> m_state;
};

}}}} // namespace cocaine::framework::detail::future

#endif // COCAINE_FRAMEWORK_FUTURE_BASIC_PROMISE_HPP
