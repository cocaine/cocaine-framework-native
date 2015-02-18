#pragma once

#include <boost/asio/ip/tcp.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

class scheduler_t;

struct resolver_result_t {
    typedef boost::asio::ip::tcp::endpoint endpoint_type;

    std::vector<endpoint_type> endpoints;
    unsigned int version;
};

/*!
 * \thread_safety all methods are reentrant.
 */
class resolver_t {
public:
    typedef resolver_result_t::endpoint_type endpoint_type;

private:
    class impl;
    std::unique_ptr<impl> d;

public:
    /*!
     * \todo \note set default endpoint to [::]:10053.
     */
    explicit resolver_t(scheduler_t& scheduler);

    ~resolver_t();

    void timeout(std::chrono::milliseconds) {}
    void endpoints(std::vector<endpoint_type> endpoints) {}

    // No queue.
    auto resolve(std::string name) -> future_type<resolver_result_t>;
};

}

}
