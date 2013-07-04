#ifndef COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP
#define COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP

#include <cocaine/framework/service.hpp>

namespace cocaine { namespace framework {

struct storage_service_t :
    public service_t
{
    static const unsigned int version = cocaine::io::protocol<cocaine::io::storage_tag>::version::value;

    storage_service_t(std::shared_ptr<service_connection_t> connection) :
        service_t(connection)
    {
        // pass
    }

    service_traits<cocaine::io::storage::read>::future_type
    read(const std::string& collection,
         const std::string& key)
    {
        return call<cocaine::io::storage::read>(collection, key);
    }

    service_traits<cocaine::io::storage::write>::future_type
    write(const std::string& collection,
          const std::string& key,
          const std::string& value)
    {
        return call<cocaine::io::storage::write>(collection, key, value);
    }

    service_traits<cocaine::io::storage::write>::future_type
    write(const std::string& collection,
          const std::string& key,
          const std::string& value,
          const std::vector<std::string>& tags)
    {
        return call<cocaine::io::storage::write>(collection, key, value, tags);
    }

    service_traits<cocaine::io::storage::remove>::future_type
    remove(const std::string& collection,
           const std::string& key)
    {
        return call<cocaine::io::storage::remove>(collection, key);
    }

    service_traits<cocaine::io::storage::find>::future_type
    find(const std::string& collection,
         const std::vector<std::string>& tags)
    {
        return call<cocaine::io::storage::find>(collection, tags);
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP
