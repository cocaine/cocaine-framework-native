#include "cocaine/framework/service.hpp"

#include "cocaine/framework/resolver.hpp"

using namespace cocaine::framework;

class basic_service_t::impl {
public:
    std::string name;
    scheduler_t& scheduler;
    resolver_t resolver;
    std::mutex mutex;

    impl(std::string name, scheduler_t& scheduler) :
        name(std::move(name)),
        scheduler(scheduler),
        resolver(scheduler)
    {}
};

basic_service_t::basic_service_t(std::string name, scheduler_t& scheduler) :
    d(new impl(std::move(name), scheduler)),
    sess(std::make_shared<session<>>(scheduler))
{}

basic_service_t::~basic_service_t() {}

auto basic_service_t::name() const -> const std::string& {
    return d->name;
}

auto basic_service_t::connect() -> future_t<void> {
    // TODO: Connector, which manages future queue and returns future<shared_ptr<session<T>>> for 'name'.
    CF_CTX("service '%s'", name());
    CF_CTX("connect");
    CF_DBG("connecting ...");

    // TODO: Make async.
    std::lock_guard<std::mutex> lock(d->mutex);

    // Internally the session manages with connection state itself. On any network error it
    // should drop its internal state and return false.
    if (sess->connected()) {
        CF_DBG("already connected");
        return make_ready_future<void>::value();
    }

    try {
        try {
            auto info = d->resolver.resolve(d->name).get();
            // TODO: Check version.
            CF_DBG("version: %d", info.version);

            sess->connect(info.endpoints).get();
            CF_DBG("connecting - done");
        } catch (std::exception err) {
            CF_DBG("connecting - error: %s", err.what());
            throw;
        }
        return make_ready_future<void>::value();
    } catch (const std::runtime_error& err) {
        return make_ready_future<void>::error(err);
    }
}
