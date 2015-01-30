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
//     * Pack args in the message and push it through session pointer.
//     *
//     * \return a future, which will be set when the message is completely sent or any network error
//     * occured.
//     *
//     * This future can throw:
//     *  - \sa encoder_error if unabled to properly pack given arguments.
//     *  - \sa network_error if the channel is in disconnected state.
//     *
//     * Setting an error in the receiver doesn't work, because there can be mute slots, which
//     * doesn't respond ever.
//     */
//    template<class Event, class... Args>
//    auto send(Args&&...) -> future<void>;
//};

//template<class T>
//class sender<T> {
//public:
//    /*!
//     * Encode arguments to the internal protocol message and push it into the session attached.
//     *
//     * \warning this sender will be invalidated after this call.
//     */
//    template<class Event, class... Args>
//    auto send(Args&&...) && -> future<sender<traverse<T, Event>>>;
//};

//class sender<void> {};

//template<class Event>
//class basic_receiver {
//    typedef result_of<Event>::type result_type;

//public:
//    auto recv() -> future<result_type>;
//};

//template<class Event>
//class receiver {
//public:
//    /*!
//     * \note does auto revoke when reached a leaf.
//     *
//     * \warning this receiver will be invalidatef after this call.
//     */
//    auto recv() && -> future<std::tuple<traverse<receiver<Event>>, result_type>>;
//};

//class receiver<void> {};

//class basic_session {
//    auto connected() const -> bool;

//    /*!
//     * Do not manage with connection queue.
//     * The future returned can be EALREADY and EISCONN, but never throws.
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

//template<class T>
//class session {
//    auto connected() const -> bool;

//    /*!
//     * Tries to connect to the given endpoint and returts future, which will be ready when
//     * connected.
//     *
//     * \note this method internally manages with the client queue. It's okay to call this method
//     * while either in disconnected, connecting or connected state.
//     * The future returned throws only on network error.
//     */
//    auto connect(endpoint_t) -> future<void>;

//    template<class Event, class... Args>
//    auto invoke(Args&&... args) -> future<std::tuple<sender<T>, receiver<T, Event>>>;
//};

//template<class T>
//class service {
//    /*!
//     * \note blocks unless detached.
//     */
//    ~service();

//    /*!
//     * \note does auto resolving.
//     */
//    auto connect() -> future<void>;
//    auto disconnect();
//    auto detach();

//    /*!
//     * \note this method automatically connects to the given endpoints if the internal session is
//     * in disconnected state.
//     */
//    auto invoke(...) -> future<tx, rx>;
//};
