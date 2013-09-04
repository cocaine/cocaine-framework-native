#ifndef COCAINE_FRAMEWORK_FUTURE_UTILITY_HPP
#define COCAINE_FRAMEWORK_FUTURE_UTILITY_HPP

#include <cocaine/framework/future/future.hpp>

#include <vector>
#include <boost/range.hpp>

namespace cocaine { namespace framework {

namespace detail { namespace future {

// when_any implementation
template<class... Futures>
class when_any_tuple_state;

template<unsigned int N, class... Futures>
struct any_subscriber {
    static
    inline
    void
    subscribe(const std::shared_ptr<when_any_tuple_state<Futures...>>& s) {
        typedef typename std::tuple_element<N, std::tuple<Futures...>>::type
                current_future;

        std::unique_lock<std::recursive_mutex> lock(s->m_futures_mutex);

        if (!s->m_cake) {
            auto& f = std::get<N>(s->m_futures);

            void(when_any_tuple_state<Futures...>::*method)(current_future&)
                            = &when_any_tuple_state<Futures...>::make_ready;
            f.when_ready(executor_t(),
                         std::bind(method, s, std::placeholders::_1));

            lock.unlock();

            any_subscriber<N - 1, Futures...>::subscribe(s);
        }
    }
};

template<class... Futures>
struct any_subscriber<0, Futures...> {
    static
    inline
    void
    subscribe(const std::shared_ptr<when_any_tuple_state<Futures...>>& s) {
        typedef typename std::tuple_element<0, std::tuple<Futures...>>::type
                current_future;

        std::unique_lock<std::recursive_mutex> lock(s->m_futures_mutex);

        if (!s->m_cake) {
            void(when_any_tuple_state<Futures...>::*method)(current_future&)
                            = &when_any_tuple_state<Futures...>::make_ready;

            std::get<0>(s->m_futures).when_ready(executor_t(),
                                                 std::bind(method, s, std::placeholders::_1));
        }
    }
};

template<class... Futures>
class when_any_tuple_state :
    public std::enable_shared_from_this<when_any_tuple_state<Futures...>>
{
    template<unsigned int, class...>
    friend struct any_subscriber;

public:
    when_any_tuple_state(Futures&&... futures) :
        m_futures(std::move(futures)...),
        m_cake(false)
    {
        // pass
    }

    cocaine::framework::future<Futures...>
    subscribe() {
        if (sizeof...(Futures) == 0) {
            m_promise.set_value(std::move(m_futures));
        } else {
            any_subscriber<sizeof...(Futures) - 1, Futures...>::subscribe(this->shared_from_this());
        }

        return m_promise.get_future();
    }

    template<class Future>
    void
    make_ready(Future& f) {
        std::unique_lock<std::recursive_mutex> lock(m_futures_mutex);
        if (!m_cake) {
            m_cake = true;
            lock.unlock();
            m_promise.set_value(std::move(m_futures));
        }
    }

private:
    promise<Futures...> m_promise;
    std::tuple<Futures...> m_futures;
    bool m_cake;
    std::recursive_mutex m_futures_mutex;
};

template<class T>
class when_any_vector_state :
    public std::enable_shared_from_this<when_any_vector_state<T>>
{
public:
    when_any_vector_state(std::vector<T>&& futures) :
        m_futures(std::move(futures)),
        m_cake(false)
    {
        // pass
    }

    cocaine::framework::future<std::vector<T>>
    subscribe() {
        if (m_futures.size() == 0) {
            m_promise.set_value(std::move(m_futures));
        } else {
            for (size_t i = 0; i < m_futures.size(); ++i) {
                std::unique_lock<std::recursive_mutex> lock(m_futures_mutex);

                if (!m_cake) {
                    m_futures[i].when_ready(executor_t(),
                                            std::bind(&when_any_vector_state<T>::make_ready,
                                                      this->shared_from_this(),
                                                      std::placeholders::_1));
                } else {
                    break;
                }
            }
        }

        return m_promise.get_future();
    }

    void
    make_ready(T& f) {
        std::unique_lock<std::recursive_mutex> lock(m_futures_mutex);
        if (!m_cake) {
            m_cake = true;
            lock.unlock();
            m_promise.set_value(std::move(m_futures));
        }
    }

private:
    promise<std::vector<T>> m_promise;
    std::vector<T> m_futures;
    bool m_cake;
    std::recursive_mutex m_futures_mutex;
};

// when_all implementation

template<class... Futures>
class when_all_tuple_state;

template<unsigned int N, class... Futures>
struct all_subscriber {
    static
    inline
    void
    subscribe(const std::shared_ptr<when_all_tuple_state<Futures...>>& s) {
        typedef typename std::tuple_element<N, std::tuple<Futures...>>::type
                current_future;

        auto& f = std::get<N>(s->m_futures);

        void(when_all_tuple_state<Futures...>::*method)(current_future&)
                        = &when_all_tuple_state<Futures...>::make_ready;
        f.when_ready(executor_t(),
                     std::bind(method, s, std::placeholders::_1));
        all_subscriber<N - 1, Futures...>::subscribe(s);
    }
};

template<class... Futures>
struct all_subscriber<0, Futures...> {
    static
    inline
    void
    subscribe(const std::shared_ptr<when_all_tuple_state<Futures...>>& s) {
        typedef typename std::tuple_element<0, std::tuple<Futures...>>::type
                current_future;

        void(when_all_tuple_state<Futures...>::*method)(current_future&)
                        = &when_all_tuple_state<Futures...>::make_ready;

        std::get<0>(s->m_futures).when_ready(executor_t(),
                                             std::bind(method, s, std::placeholders::_1));
    }
};

template<class... Futures>
class when_all_tuple_state :
    public std::enable_shared_from_this<when_all_tuple_state<Futures...>>
{
    template<unsigned int, class...>
    friend struct all_subscriber;

public:
    when_all_tuple_state(Futures&&... futures) :
        m_futures(std::move(futures)...),
        m_cake(sizeof...(Futures))
    {
        // pass
    }

    cocaine::framework::future<Futures...>
    subscribe() {
        if (sizeof...(Futures) == 0) {
            m_promise.set_value(std::move(m_futures));
        } else {
            all_subscriber<sizeof...(Futures) - 1, Futures...>::subscribe(this->shared_from_this());
        }

        return m_promise.get_future();
    }

    template<class Future>
    void
    make_ready(Future& f) {
        unsigned int counter = --(this->m_cake);
        if (counter == 0) {
            m_promise.set_value(std::move(m_futures));
        }
    }

private:
    promise<Futures...> m_promise;
    std::tuple<Futures...> m_futures;
    std::atomic<unsigned int> m_cake;
};

template<class T>
class when_all_vector_state :
    public std::enable_shared_from_this<when_all_vector_state<T>>
{
public:
    when_all_vector_state(std::vector<T>&& futures) :
        m_futures(std::move(futures)),
        m_cake(m_futures.size())
    {
        // pass
    }

    cocaine::framework::future<std::vector<T>>
    subscribe() {
        if (m_futures.size() == 0) {
            m_promise.set_value(std::move(m_futures));
        } else {
            for (size_t i = 0; i < m_futures.size(); ++i) {
                m_futures[i].when_ready(executor_t(),
                                        std::bind(&when_all_vector_state<T>::make_ready,
                                                  this->shared_from_this(),
                                                  std::placeholders::_1));
            }
        }

        return m_promise.get_future();
    }

    template<class Future>
    void
    make_ready(Future& f) {
        unsigned int counter = --(this->m_cake);
        if (counter == 0) {
            m_promise.set_value(std::move(m_futures));
        }
    }

private:
    promise<std::vector<T>> m_promise;
    std::vector<T> m_futures;
    std::atomic<unsigned int> m_cake;
};

}} // namespace detail::future

template<class... Futures>
typename std::enable_if<
    (sizeof...(Futures) > 1),
    future<typename std::decay<Futures>::type...>
>::type
when_any(Futures&&... futures) {
    typedef detail::future::when_any_tuple_state<typename std::decay<Futures>::type...>
            state_type;

    auto state = std::make_shared<state_type>(std::forward<Futures>(futures)...);

    return state->subscribe();
}

template<class... Futures>
typename std::enable_if<
    (sizeof...(Futures) > 1),
    future<typename std::decay<Futures>::type...>
>::type
when_all(Futures&&... futures) {
    typedef detail::future::when_all_tuple_state<typename std::decay<Futures>::type...>
            state_type;

    auto state = std::make_shared<state_type>(std::forward<Futures>(futures)...);

    return state->subscribe();
}

template<class Range>
future<std::vector<typename boost::range_value<Range>::type>>
when_any(Range& range) {
    std::vector<typename boost::range_value<Range>::type> result;

    for (auto it = boost::begin(range); it != boost::end(range); ++it) {
        result.emplace_back(std::move(*it));
    }

    typedef detail::future::when_any_vector_state<typename boost::range_value<Range>::type>
            state_type;

    auto state = std::make_shared<state_type>(std::move(result));

    return state->subscribe();
}

template<class Range>
future<std::vector<typename boost::range_value<Range>::type>>
when_all(Range& range) {
    std::vector<typename boost::range_value<Range>::type> result;

    for (auto it = boost::begin(range); it != boost::end(range); ++it) {
        result.emplace_back(std::move(*it));
    }

    typedef detail::future::when_all_vector_state<typename boost::range_value<Range>::type>
            state_type;

    auto state = std::make_shared<state_type>(std::move(result));

    return state->subscribe();
}

}} // namespace cocaine::framework


#endif // COCAINE_FRAMEWORK_FUTURE_UTILITY_HPP
