#pragma once
#include <cocaine/trace/trace.hpp>
#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/forwards.hpp"
namespace cocaine { namespace framework {

class service_logger_t {
public:
    class impl;
    ~service_logger_t();

    /*
     * Valid only when manager is in scope, so lifetime should be controlled manually.
     */
    service_logger_t(framework::service_manager_t& manager, std::string service_name);

    void
    log(std::string message, blackhole::attribute::set_t attributes);

private:
    std::unique_ptr<impl> d;
};

}}

