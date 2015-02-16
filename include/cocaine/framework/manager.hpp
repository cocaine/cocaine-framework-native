#pragma once

#include <memory>

#include <boost/optional.hpp>
#include <boost/thread.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

class service_manager_t {
    loop_t loop;
    boost::optional<loop_t::work> work;
    boost::thread_group pool;

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

    loop_t& next();
};

}

} // namespace cocaine::framework
