#pragma once

#include <string>

#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace cocaine {

namespace framework {

using loop_t = boost::asio::io_service;
using endpoint_t = boost::asio::ip::tcp::endpoint;

template<class T>
class low_level_service {
    // TODO: Static Assert - T contains protocol tag.

    loop_t& loop;
public:
    /*!
     * \note the event loop reference should be valid until all asynchronous operations complete
     * otherwise the behavior is undefined.
     */
    low_level_service(std::string name, loop_t& loop) noexcept :
        loop(loop)
    {}

    boost::future<void> connect(endpoint_t endpoint) {
        std::shared_ptr<boost::promise<void>> promise(new boost::promise<void>());
        boost::future<void> future = promise->get_future();
        loop.post([promise]{ promise->set_value(); });
        return future;
    }

    bool connected() const noexcept {
        return false;
    }
};

} // namespace framework

} // namespace cocaine
