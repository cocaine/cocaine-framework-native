#pragma once

#include <boost/asio/ip/tcp.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

struct resolver_result_t {
    typedef boost::asio::ip::tcp::endpoint endpoint_type;

    std::vector<endpoint_type> endpoints;
    unsigned int version;
};

/*!
 * \reentrant
 */
class resolver_t {
public:
    typedef resolver_result_t::endpoint_type endpoint_type;

private:
    class impl;
    std::unique_ptr<impl> d;

public:
    /*!
     * \note sets default endpoint to [::]:10053.
     */
    explicit resolver_t(scheduler_t& scheduler);

    ~resolver_t();

    void timeout(std::chrono::milliseconds);
    void endpoints(std::vector<endpoint_type> endpoints);

    // No queue.
    auto resolve(std::string name) -> typename task<resolver_result_t>::future_type;
};

}

}
