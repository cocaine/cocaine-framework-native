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
                      const cocaine::io::tcp::endpoint& resolver) :
        service_t(name, service, resolver)
    {
        // pass
    }

    template<class F>
    void
    read(F callback,
         const std::string& collection,
         const std::string& key)
    {
        call<io::storage::read>(callback, collection, key);
    }

    template<class F>
    void
    write(F callback,
          const std::string& collection,
          const std::string& key,
          const std::string& value)
    {
        call<io::storage::write>(callback, collection, key, value);
    }

    template<class F>
    void
    remove(F callback,
           const std::string& collection,
           const std::string& key)
    {
        call<io::storage::remove>(callback, collection, key);
    }

    template<class F>
    void
    list(F callback,
         const std::string& collection)
    {
        call<io::storage::list>(callback, collection);
    }
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_STORAGE_HPP
