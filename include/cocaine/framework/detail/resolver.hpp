#pragma once

#include <unordered_map>

#include <boost/asio/ip/tcp.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

namespace detail {

/*!
 * \reentrant
 */
class resolver_t {
public:
    typedef boost::asio::ip::tcp::endpoint endpoint_type;

    struct result_t {
        std::vector<endpoint_type> endpoints;
        unsigned int version;
    };

private:
    scheduler_t& scheduler;
    std::vector<endpoint_type> endpoints_;

public:
    /*!
     * \note sets default endpoint to [::]:10053.
     */
    explicit resolver_t(scheduler_t& scheduler);

    ~resolver_t();

    // TODO: Unnecessary, use futures.
    void timeout(std::chrono::milliseconds);

    std::vector<endpoint_type> endpoints() const;
    void endpoints(std::vector<endpoint_type> endpoints);

    // No queue.
    auto resolve(std::string name) -> task<result_t>::future_type;
};

/// Manages with queue.
/// \threadsafe
class serialized_resolver_t : public std::enable_shared_from_this<serialized_resolver_t> {
public:
    typedef resolver_t::result_t result_type;
    typedef resolver_t::endpoint_type endpoint_type;

private:
    resolver_t resolver;
    scheduler_t& scheduler;
    std::unordered_map<std::string, std::deque<task<result_type>::promise_type>> inprogress;
    std::mutex mutex;

public:
    serialized_resolver_t(std::vector<endpoint_type> endpoints, scheduler_t& scheduler);

    auto resolve(std::string name) -> task<result_type>::future_type;

    result_type
    notify_all(task<result_type>::future_move_type future, std::string name);
};

} // namespace detail

} // namespace framework

} // namespace cocaine
