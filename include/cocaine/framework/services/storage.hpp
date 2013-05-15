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
                      std::shared_ptr<logger_t> logger) :
        service_t(name,
                  service,
                  resolver,
                  logger,
                  cocaine::io::protocol<cocaine::io::storage_tag>::version::value)
    {
        // pass
    }

    service_t::handler<cocaine::io::storage::read>::future
    read(const std::string& collection,
         const std::string& key)
    {
        return call<cocaine::io::storage::read>(collection, key);
    }

    service_t::handler<cocaine::io::storage::write>::future
    write(const std::string& collection,
          const std::string& key,
          const std::string& value)
    {
        return call<cocaine::io::storage::write>(collection, key, value);
    }

    service_t::handler<cocaine::io::storage::write>::future
    write(const std::string& collection,
          const std::string& key,
          const std::string& value,
          const std::vector<std::string>& tags)
    {
        return call<cocaine::io::storage::write>(collection, key, value, tags);
    }

    service_t::handler<cocaine::io::storage::remove>::future
    remove(const std::string& collection,
           const std::string& key)
    {
        return call<cocaine::io::storage::remove>(collection, key);
    }

    service_t::handler<cocaine::io::storage::find>::future
    find(const std::string& collection,
         const std::vector<std::string>& tags)
    {
        return call<cocaine::io::storage::find>(collection, tags);
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP
