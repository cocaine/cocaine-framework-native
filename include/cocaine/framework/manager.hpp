#pragma once

#include <memory>

#include <boost/optional.hpp>
#include <boost/thread.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

class execution_unit_t;

class service_manager_t {
    size_t current;
    boost::thread_group pool;
    std::vector<std::unique_ptr<execution_unit_t>> units;

public:
    service_manager_t();
    explicit service_manager_t(unsigned int threads);

    ~service_manager_t();

    template<class Service, class... Args>
    std::shared_ptr<Service> create(const std::string& name) {
        return std::make_shared<Service>(name, next());
    }

private:
    void start(unsigned int threads);

    event_loop_t& next();
};

} // namespace framework

} // namespace cocaine
