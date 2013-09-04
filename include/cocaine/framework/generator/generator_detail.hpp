#ifndef COCAINE_FRAMEWORK_GENERATOR_DETAIL_HPP
#define COCAINE_FRAMEWORK_GENERATOR_DETAIL_HPP

#include <cocaine/framework/generator/shared_state.hpp>

#include <vector>

namespace cocaine { namespace framework {

template<class... Args>
class generator;

namespace detail { namespace generator {

// create generator from shared_state
template<class... Args>
inline
cocaine::framework::generator<Args...>
generator_from_state(const std::shared_ptr<shared_state<Args...>>& state,
                     executor_t executor = executor_t())
{
    return cocaine::framework::generator<Args...>(state, executor);
}

template<class F, class Future>
struct unwrapped_result {
    typedef decltype(Future().unwrap())
            arg_type;
    typedef decltype(declval<F>()(declval<arg_type&>()))
            result_type;
    typedef decltype(cocaine::framework::future<result_type>().unwrap())
            type;
};

template<class Result, class... Args>
class map_stream:
    public generator_traits<Args...>::stream_type
{
    typedef cocaine::framework::future<Args...>
            future_type;

    typedef decltype(future_type().unwrap())
            unwrapped_type;

    typedef std::function<Result(unwrapped_type&)>
            task_type;

    typedef decltype(cocaine::framework::future<Result>().unwrap())
            result_type;

public:
    map_stream(executor_t executor,
               task_type task) :
        m_closed(false),
        m_executor(executor),
        m_callback(task),
        m_consumer(new shared_state<result_type>)
    {
        // pass
    }

    void
    write(typename future_traits<Args...>::storable_type&& value) {
        auto f = make_ready_future<Args...>::value(std::move(value)).unwrap();
        f.set_default_executor(m_executor);
        m_consumer->try_write(f.then(m_callback));
    }

    void
    error(const std::exception_ptr& e) {
        close();

        auto f = make_ready_future<Args...>::error(e).unwrap();
        f.set_default_executor(m_executor);
        m_consumer->try_write(f.then(m_callback));
    }

    void
    close() {
        m_closed = true;
    }

    bool
    closed() const {
        return m_closed;
    }

    cocaine::framework::generator<result_type>
    get_generator() {
        return generator_from_state<result_type>(m_consumer);
    }

private:
    bool m_closed;
    executor_t m_executor;
    task_type m_callback;
    std::shared_ptr<shared_state<result_type>> m_consumer;
};

template<class... Args>
class gather_stream:
    public generator_traits<Args...>::stream_type
{
    typedef typename future_traits<Args...>::storable_type
            value_type;

public:
    typedef std::vector<value_type>
            result_type;

    gather_stream() :
        m_closed(false)
    {
        // pass
    }

    void
    write(value_type&& value) {
        m_values.emplace_back(std::move(value));
    }

    void
    error(const std::exception_ptr& e) {
        m_closed = true;
        m_promise.set_exception(e);
    }

    void
    close() {
        m_closed = true;
        m_promise.set_value(std::move(m_values));
    }

    bool
    closed() const {
        return m_closed;
    }

    cocaine::framework::future<result_type>
    get_future() {
        return m_promise.get_future();
    }

private:
    bool m_closed;
    result_type m_values;
    promise<result_type> m_promise;
};

template<>
class gather_stream<void>:
    public generator_traits<void>::stream_type
{
public:
    typedef void
            result_type;

    gather_stream() :
        m_closed(false)
    {
        // pass
    }

    void
    error(const std::exception_ptr& e) {
        m_closed = true;
        m_promise.set_exception(e);
    }

    void
    close() {
        m_closed = true;
        m_promise.set_value();
    }

    bool
    closed() const {
        return m_closed;
    }

    cocaine::framework::future<void>
    get_future() {
        return m_promise.get_future();
    }

private:
    bool m_closed;
    promise<void> m_promise;
};

}} // namespace detail::generator

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_GENERATOR_DETAIL_HPP
