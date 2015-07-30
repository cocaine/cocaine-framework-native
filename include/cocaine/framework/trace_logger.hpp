#pragma once
#include <cocaine/trace/trace.hpp>
#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/forwards.hpp"

namespace cocaine { namespace io {
    struct log_tag;
}} // namespace cocaine::io

namespace cocaine { namespace framework {

class internal_logger_t
{
public:
    class impl;
    ~internal_logger_t();

    /**
     * Valid only when manager is in scope, so lifetime should be controlled manually.
     */
    internal_logger_t(std::shared_ptr<service<io::log_tag>> logger_service);

    internal_logger_t(internal_logger_t&&);

    void
    log(std::string message);

private:
    /**
     * Constructs an empty object that does nothing
     */
    internal_logger_t();

    friend class service_manager_data;

    std::unique_ptr<impl> d;
};

}}

