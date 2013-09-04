#ifndef COCAINE_FRAMEWORK_FUTURE_PROMISE_HPP
#define COCAINE_FRAMEWORK_FUTURE_PROMISE_HPP

#include <cocaine/framework/future/basic_promise.hpp>

namespace cocaine { namespace framework {

template<class... Args>
class promise :
    public detail::future::basic_promise<Args...>
{
public:
    typedef future<Args...> future_type;

    promise()
    {
        // pass
    }

    promise(promise&& other) :
        detail::future::basic_promise<Args...>(std::move(other))
    {
        // pass
    }

    promise&
    operator=(promise&& other) {
        detail::future::basic_promise<Args...>::operator=(std::move(other));
        return *this;
    }

    template<class... Args2>
    void
    set_value(Args2&&... args) {
        if (this->m_state) {
            this->m_state->set_value(std::forward<Args2>(args)...);
        } else {
            throw future_error(future_errc::no_state);
        }
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_FUTURE_PROMISE_HPP
