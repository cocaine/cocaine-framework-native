#include "cocaine/framework/service.hpp"

using namespace cocaine::framework;

class basic_service_t::impl {
public:
    std::string name;
    scheduler_t& scheduler;
    std::mutex mutex;

    impl(std::string name, scheduler_t& scheduler) :
        name(std::move(name)),
        scheduler(scheduler)
    {}
};

basic_service_t::basic_service_t(std::string name, scheduler_t& scheduler) :
    d(new impl(std::move(name), scheduler)),
    sess(std::make_shared<session<>>(scheduler))
{}

basic_service_t::~basic_service_t() {}

auto basic_service_t::name() const -> const std::string&{
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

    // connector.connect(name).then(executor, [](d){ lock; if !this->d.connected() this->d = d });
    try {
        CF_DBG("connecting to the locator ...");
        // TODO: Explicitly set Locator endpoints.
        const boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 10053);
        try {
            session<> locator(d->scheduler);
            locator.connect(endpoint).get();
            // TODO: Simplify that shit.
            CF_DBG("resolving");
            auto ch = locator.invoke<io::locator::resolve>(d->name).get();
            auto rx = std::move(std::get<1>(ch));
            auto result = rx.recv().get();
            CF_DBG("resolving - done");
            // TODO: Check version.

            auto endpoints = std::get<0>(result);
            auto version = std::get<1>(result);
            CF_DBG("version: %d", version);

            sess->connect(util::endpoint_cast(endpoints[0])).get();
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
