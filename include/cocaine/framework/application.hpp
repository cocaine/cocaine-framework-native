#ifndef COCAINE_FRAMEWORK_APPLICATION_HPP
#define COCAINE_FRAMEWORK_APPLICATION_HPP

#include <cocaine/framework/basic_application.hpp>
#include <cocaine/framework/handlers/functional.hpp>

namespace cocaine { namespace framework {

template<class App>
class application :
    public std::enable_shared_from_this<App>,
    public basic_application_t
{
public:
    application(const std::string& name,
                std::shared_ptr<service_manager_t> service_manager) :
        basic_application_t(name, service_manager)
    {
        // pass
    }

    virtual
    void
    initialize() = 0;

    using basic_application_t::on;
    using basic_application_t::on_unregistered;

    void
    on(const std::string& event,
       std::string(App::*)(const std::string&, const std::vector<std::string>&));

    void
    on(const std::string& event,
       std::function<std::string(const std::string&, const std::vector<std::string>&)>);

    template<class UserHandler>
    void
    on(const std::string& event);

    template<class Factory>
    void
    on(const std::string& event,
       std::shared_ptr<Factory> factory);

    void
    on_unregistered(std::string(App::*)(const std::string&, const std::vector<std::string>&));

    void
    on_unregistered(std::function<std::string(const std::string&, const std::vector<std::string>&)>);

    template<class UserHandler>
    void
    on_unregistered();

    template<class Factory>
    void
    on_unregistered(std::shared_ptr<Factory> factory);

    template<class Service, typename... Args>
    void
    create_service(std::shared_ptr<Service> &s,
                   Args&&... args)
    {
        s = this->service_manager()->
                service_manager_t::get_service<Service>(std::forward<Args>(args)...);
    }
};

template<class App>
class handler :
    public basic_handler_t
{
public:
    typedef App application_type;

    handler(std::shared_ptr<application_type> a) :
        m_app(a)
    {
        // pass
    }

protected:
    std::shared_ptr<application_type>
    app() const {
        return m_app;
    }

private:
    std::shared_ptr<application_type> m_app;
};

template<class UserHandler>
class handler_factory :
    public basic_factory_t
{
    friend class application<typename UserHandler::application_type>;

    typedef typename UserHandler::application_type application_type;

public:
    handler_factory()
    {
        // pass
    }

    std::shared_ptr<basic_handler_t>
    make_handler();

private:
    virtual
    void
    set_application(std::shared_ptr<application_type> a) {
        m_app = a;
    }

private:
    std::shared_ptr<application_type> m_app;
};

template<class UserHandler>
std::shared_ptr<basic_handler_t>
handler_factory<UserHandler>::make_handler() {
    if (m_app) {
        return std::make_shared<UserHandler>(m_app);
    } else {
        throw bad_factory_exception_t("Application has not been set.");
    }
}

template<class App>
void
application<App>::on(const std::string& event,
                     std::string(App::*method)(const std::string&, const std::vector<std::string>&))
{
    on(event,
       std::make_shared<method_factory<App>>(this->shared_from_this(), method));
}

template<class App>
void
application<App>::on(
    const std::string& event,
    std::function<std::string(const std::string&, const std::vector<std::string>&)> functor
)
{
    on(event,
       std::make_shared<function_factory_t>(functor));
}

template<class App>
template<class UserHandler>
void
application<App>::on(const std::string& event) {
    handler_factory<UserHandler> *new_factory = new handler_factory<UserHandler>;
    new_factory->set_application(this->shared_from_this());
    on(event, std::shared_ptr<basic_factory_t>(new_factory));
}

template<class App>
template<class Factory>
void
application<App>::on(const std::string& event,
                     std::shared_ptr<Factory> factory)
{
    on(event,
       std::static_pointer_cast<basic_factory_t>(factory));
}

template<class App>
void
application<App>::on_unregistered(
    std::string(App::*method)(const std::string&, const std::vector<std::string>&)
)
{
    on_unregistered(std::make_shared<method_factory<App>>(this->shared_from_this(), method));
}

template<class App>
void
application<App>::on_unregistered(
    std::function<std::string(const std::string&, const std::vector<std::string>&)> functor
)
{
    on_unregistered(std::make_shared<function_factory_t>(functor));
}

template<class App>
template<class UserHandler>
void
application<App>::on_unregistered() {
    handler_factory<UserHandler> *new_factory = new handler_factory<UserHandler>;
    new_factory->set_application(this->shared_from_this());
    on_unregistered(std::shared_ptr<basic_factory_t>(new_factory));
}

template<class App>
template<class Factory>
void
application<App>::on_unregistered(std::shared_ptr<Factory> factory)
{
    on_unregistered(std::static_pointer_cast<basic_factory_t>(factory));
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_APPLICATION_HPP
