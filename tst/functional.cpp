#include <gtest/gtest.h>

#include <cocaine/idl/node.hpp>
#include <cocaine/rpc/upstream.hpp>

#include <cocaine/framework/connection.hpp>

using namespace cocaine::framework;

class basic_sender {
    uint64_t span;

public:
    basic_sender(uint64_t);

    /*!
     * Pack args and push message through session pointer.
     *
     * \return a future, which will be set when the message is completely sent or any network error
     * occured.
     *
     * \throw encoder_error if unable to properly pack given arguments.
     * \throw network_error in the returned future if the channel is in disconnected state.
     * Setting an error in the receiver doesn't work, because there can be mute slots, which
     * doesn't respond ever.
     */
    template<class Event, class... Args>
    auto send(Args&&...) -> future<void>;
};

//class sender<T> {
//public:
//    template<class Event, class... Args>
//    auto send(Args&&...) -> sender<traverse<Event>>;
//};

//class sender<void> {};

//template<class Event>
//class basic_receiver {
//    typedef result_of<Event>::type result_type;

//public:
//    auto recv() -> future<result_type>;

//private:
//    auto push(message&&) -> void;
//    auto push(std::system_error) -> void;
//};

//class receiver<Event> {
//public:
//    // Auto revoke when reached a leaf.
//    auto recv() -> std::tuple<receiver<traverse<Event>>, result_type>;
//};

class basic_session {
    auto connected() const -> bool;

    /*!
     * Do not manage with connection queue.
     * The future returned can be EALREADY and EISCONN.
     */
    auto connect(endpoint_t) -> future<std::system_error>;

    /*!
     * \brief disconnect
     * Stops all pending asynchronous operations. All futures will throw operation_aborted error.
     */
    auto disconnect() -> void;

    template<class Event, class... Args>
    auto invoke(Args&&...) -> std::tuple<basic_sender, basic_receiver<Event>>;
    auto revoke(uint64_t) -> std::shared_ptr<basic_channel_t>;
};

//class session {
//    void connect();

//    template<class Event, class... Args>
//    invoke(Args&&... args);
//};
