#ifndef COCAINE_FRAMEWORK_SERVICE_STATUS_HPP
#define COCAINE_FRAMEWORK_SERVICE_STATUS_HPP

namespace cocaine { namespace framework {

enum class service_status {
    disconnected,
    connecting,
    connected,
    not_found
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_STATUS_HPP
