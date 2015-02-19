#pragma once

/// Use internal logging system (much verbose, so spam, wow).
#define CF_USE_INTERNAL_LOGGING

#define USE_PURE_ASIO

#ifdef USE_PURE_ASIO
    #include <asio/io_service.hpp>
#else
    #include <boost/asio/io_service.hpp>
#endif

namespace cocaine {

namespace framework {

typedef asio::io_service loop_t;

} // namespace framework

} // namespace cocaine
