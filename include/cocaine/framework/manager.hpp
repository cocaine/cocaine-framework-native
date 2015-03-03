#pragma once

#include <memory>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

class execution_unit_t;

class service_manager_t {
    size_t current;
    std::vector<std::unique_ptr<execution_unit_t>> units;

public:
    service_manager_t();
    explicit service_manager_t(unsigned int threads);

    ~service_manager_t();

    template<class T, class... Args>
    std::shared_ptr<service<T>> create(std::string name) {
        return std::make_shared<service<T>>(std::move(name), next());
    }

private:
    void start(unsigned int threads);

    scheduler_t& next();
};

} // namespace framework

} // namespace cocaine
