#ifndef COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP
#define COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP

#include <cocaine/framework/service.hpp>

namespace cocaine { namespace framework {

class storage_service_t :
    public service_t
{
public:
    storage_service_t(const std::string& name,
                      cocaine::io::reactor_t& service,
                      const cocaine::io::tcp::endpoint& resolver,
                      std::shared_ptr<logging_service_t> logger) :
        service_t(name,
                  service,
                  resolver,
                  logger,
                  cocaine::io::protocol<cocaine::io::storage_tag>::version::value)
    {
        // pass
    }

    service_t::handler<io::storage::read>::future
    read(const std::string& collection,
         const std::string& key)
    {
        return call<io::storage::read>(collection, key);
    }

    service_t::handler<io::storage::write>::future
    write(const std::string& collection,
          const std::string& key,
          const std::string& value)
    {
        return call<io::storage::write>(collection, key, value);
    }

    service_t::handler<io::storage::remove>::future
    remove(const std::string& collection,
           const std::string& key)
    {
        return call<io::storage::remove>(collection, key);
    }

    service_t::handler<io::storage::list>::future
    list(const std::string& collection)
    {
        return call<io::storage::list>(collection);
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP
