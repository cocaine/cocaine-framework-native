#include <gtest/gtest.h>

#include <blackhole/attribute.hpp>

#include <cocaine/common.hpp>
#include <cocaine/idl/logging.hpp>
#include <cocaine/traits/enum.hpp>

#include <cocaine/framework/manager.hpp>
#include <cocaine/framework/service.hpp>

using namespace cocaine;
using namespace cocaine::framework;

TEST(service, Logging) {
    service_manager_t manager(1);
    auto logger = manager.create<io::log_tag>("logging");
    logger.invoke<io::log::emit>(logging::debug, std::string("app/testing"), std::string("le message")).get();
}
