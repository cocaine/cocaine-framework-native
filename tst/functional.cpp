//#include <gtest/gtest.h>

//#include <cocaine/idl/node.hpp>
//#include <cocaine/rpc/upstream.hpp>

//#include <cocaine/framework/connection.hpp>

//using namespace cocaine::framework;

//class basic_sender {
//    uint64_t span;

//public:
//    basic_sender(uint64_t, basic_session*);

//    /*!
//     * Pack args and push message through session pointer.
//     *
//     * \return a future, which will be set when the message is completely sent or any network error
//     * occured.
//     *
//     * \throw encoder_error in the returned future if unable to properly pack given arguments.
//     * \throw network_error in the returned future if the channel is in disconnected state.
//     * Setting an error in the receiver doesn't work, because there can be mute slots, which
//     * doesn't respond ever.
//     */
//    template<class Event, class... Args>
//    auto send(Args&&...) -> future<void>;
//};

//template<class T>
//class sender<T> {
//public:
//    template<class Event, class... Args>
//    auto send(Args&&...) -> future<sender<traverse<Event>>>;
//};

//class sender<void> {};

//template<class Event>
//class basic_receiver {
//    typedef result_of<Event>::type result_type;

//public:
//    /*!
//     * Accepts thread safe queue.
//     */
//    basic_receiver(queue*);

//    auto recv() -> future<result_type>;
//};

//template<class Event>
//class receiver {
//public:
//    /*!
//     * \note does auto revoke when reached a leaf.
//     */
//    auto recv() -> future<std::tuple<receiver<traverse<Event>>, result_type>>;
//};

//class receiver<void> {};

//class basic_session {
//    auto connected() const -> bool;

//    /*!
//     * Do not manage with connection queue.
//     * The future returned can be EALREADY and EISCONN.
//     */
//    auto connect(endpoint_t) -> future<std::system_error>;

//    /*!
//     * \brief disconnect
//     * Stops all pending asynchronous operations. All futures will throw operation_aborted error.
//     */
//    auto disconnect() -> void;

//    template<class Event, class... Args>
//    auto invoke(Args&&...) -> future<std::tuple<basic_sender, basic_receiver<Event>>>;
//    auto send(uint64_t, message) -> future<std::system_error>;
//    auto revoke(uint64_t);
//};

////class session {
////    auto connect(endpoint_t) -> future<...>;

////    template<class Event, class... Args>
////    invoke(Args&&... args);
////};

//template<class T>
//class service {
////    ~service();

//    /*!
//     * \note does auto resolving.
//     */
////    auto connect() -> future<void>;
////    auto disconnect();
////    auto detach();

//    /*!
//     * \note does auto reconnection if needed.
//     */
////    auto invoke(...) -> future<tx, rx>;
//};
