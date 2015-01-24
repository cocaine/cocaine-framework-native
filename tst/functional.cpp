#include <gtest/gtest.h>

#include <cocaine/idl/node.hpp>
#include <cocaine/rpc/upstream.hpp>

#include <cocaine/framework/connection.hpp>

using namespace cocaine::framework;

class basic_sender {
    uint64_t span;

public:
    basic_sender(uint64_t);

    template<class Event, class... Args>
    auto send(Args&&...) -> void;
};

class sender<T> {
public:
    template<class Event, class... Args>
    auto send(Args&&...) -> sender<traverse<Event>>;
};

class sender<void> {};

template<class Event>
class basic_receiver {
    typedef result_of<Event>::type result_type;

public:
    auto recv() -> future<result_type>;

private:
    auto push(message&&) -> void;
    auto push(std::system_error) -> void;
};

class receiver<Event> {
public:
    // Auto revoke when reached a leaf.
    auto recv() -> std::tuple<receiver<traverse<Event>>, result_type>;
};

class basic_session {
    auto connected() const -> bool;

    /*!
     * Do not manage with connection queue.
     * The future returned can be EALREADY and EISCONN.
     */
    auto connect(endpoint_t) -> future<std::system_error>;
    auto disconnect() -> void;

    template<class Event, class... Args>
    auto invoke(Args&&...) -> std::tuple<basic_sender, basic_receiver<Event>>;
    auto revoke(uint64_t) -> std::shared_ptr<basic_channel_t>;
};

class session {
    void connect();

    template<class Event, class... Args>
    invoke(Args&&... args);
};
