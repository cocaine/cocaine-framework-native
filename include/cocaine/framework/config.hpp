#pragma once

#define USE_PURE_ASIO

#ifdef USE_PURE_ASIO
    #include <asio/io_service.hpp>
#else
    #include <boost/asio/io_service.hpp>
#endif

namespace cocaine {

namespace framework {

#ifdef USE_PURE_ASIO
    namespace io_provider = asio;
#else
    namespace io_provider = boost::asio;
#endif

using loop_t = asio::io_service;

} // namespace framework

} // namespace cocaine
