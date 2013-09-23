#ifndef COCAINE_FRAMEWORK_SERVICE_STATUS_HPP
#define COCAINE_FRAMEWORK_SERVICE_STATUS_HPP

#include <cocaine/asio/tcp.hpp>
#include <cocaine/asio/socket.hpp>
#include <cocaine/rpc/channel.hpp>

#include <memory>
#include <string>
#include <mutex>

namespace cocaine { namespace framework {

enum class service_status {
    disconnected,
    connecting,
    connected,
    not_found
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_STATUS_HPP
