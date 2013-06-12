#ifndef COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP
#define COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP

#include <cocaine/framework/service.hpp>

namespace cocaine { namespace framework {

struct storage_service_t :
    public service_stub_t
{
    static const unsigned int version = cocaine::io::protocol<cocaine::io::storage_tag>::version::value;

    storage_service_t(std::shared_ptr<service_t> service) :
        service_stub_t(service)
    {
        // pass
    }

    service_t::handler<cocaine::io::storage::read>::future
    read(const std::string& collection,
         const std::string& key)
    {
        return backend()->call<cocaine::io::storage::read>(collection, key);
    }

    service_t::handler<cocaine::io::storage::write>::future
    write(const std::string& collection,
          const std::string& key,
          const std::string& value)
    {
        return backend()->call<cocaine::io::storage::write>(collection, key, value);
    }

    service_t::handler<cocaine::io::storage::write>::future
    write(const std::string& collection,
          const std::string& key,
          const std::string& value,
          const std::vector<std::string>& tags)
    {
        return backend()->call<cocaine::io::storage::write>(collection, key, value, tags);
    }

    service_t::handler<cocaine::io::storage::remove>::future
    remove(const std::string& collection,
           const std::string& key)
    {
        return backend()->call<cocaine::io::storage::remove>(collection, key);
    }

    service_t::handler<cocaine::io::storage::find>::future
    find(const std::string& collection,
         const std::vector<std::string>& tags)
    {
        return backend()->call<cocaine::io::storage::find>(collection, tags);
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP
