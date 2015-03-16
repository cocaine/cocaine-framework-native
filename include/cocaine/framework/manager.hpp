#pragma once

#include <memory>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/session.hpp"

namespace cocaine {

namespace framework {

class execution_unit_t;

class service_manager_t {
    size_t current;
    std::vector<session_t::endpoint_type> locations;
    std::vector<std::unique_ptr<execution_unit_t>> units;

public:
    service_manager_t();
    explicit service_manager_t(unsigned int threads);

    ~service_manager_t();

    std::vector<session_t::endpoint_type> endpoints() const;
    void endpoints(std::vector<session_t::endpoint_type> endpoints);

    template<class T>
    service<T>
    create(std::string name) {
        return service<T>(std::move(name), endpoints(), next());
    }

private:
    void start(unsigned int threads);

    scheduler_t& next();
};

} // namespace framework

} // namespace cocaine
