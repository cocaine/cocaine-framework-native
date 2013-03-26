#ifndef COCAINE_FRAMEWORK_APPLICATION_HPP
#define COCAINE_FRAMEWORK_APPLICATION_HPP

#include <stdexcept>
#include <typeinfo>
#include <memory>
#include <string>
#include <map>
#include <boost/utility.hpp>

#include <cocaine/api/stream.hpp>

#include <cocaine/framework/logging.hpp>
#include <cocaine/framework/service.hpp>
#include <cocaine/framework/services/logger.hpp>

namespace cocaine { namespace framework {

class application_t;

struct bad_factory_exception :
    public std::runtime_error
{
    explicit bad_factory_exception(const std::string& what) :
        runtime_error(what)
    {
        // pass
    }
};

struct handler_error :
    public std::runtime_error
{
    explicit handler_error(const std::string& what) :
        runtime_error(what)
    {
        // pass
    }
};

struct base_handler_t :
    public cocaine::api::stream_t,
    public boost::noncopyable
{
    enum class state_t: int {
        opened,
        closed
    };

    base_handler_t() :
        m_state(state_t::closed)
    {
        // pass
    }

    virtual
    ~base_handler_t()
    {
        close();
    }

    virtual
    void
    on_chunk(const char *chunk,
             size_t size) = 0;

    virtual
    void
    on_close() {
        // pass
    }


    virtual
    void
    invoke(const std::string& event,
           std::shared_ptr<cocaine::api::stream_t> response)
    {
        m_event = event;
        m_response = response;
        m_state = state_t::opened;
    }

    void
    write(const char *chunk,
          size_t size)
    {
        on_chunk(chunk, size);
    }

    void
    close() {
        on_close();
        m_state = state_t::closed;
    }

    bool
    closed() const {
        return m_state == state_t::closed;
    }

    void
    error(cocaine::error_code code,
          const std::string& message);

protected:
    state_t m_state;
    std::string m_event;
    std::shared_ptr<cocaine::api::stream_t> m_response;
};

class base_factory_t {
public:
    virtual
    std::shared_ptr<base_handler_t>
    make_handler() = 0;
};

template<class AppT>
struct user_handler :
    public base_handler_t
{
    typedef AppT application_type;

    user_handler(application_type& a) :
        app(a)
    {
        // pass
    }

protected:
    application_type& app;
};

template<class UserHandlerT>
class user_handler_factory :
    public base_factory_t
{
    friend class application_t;

    typedef typename UserHandlerT::application_type application_type;

public:
    user_handler_factory() :
        m_app(nullptr)
    {
        // pass
    }

    std::shared_ptr<base_handler_t>
    make_handler();

protected:
    void
    set_application(application_type *a) {
        m_app = a;
    }

protected:
    application_type *m_app;
};

template<class UserHandlerT>
std::shared_ptr<base_handler_t>
user_handler_factory<UserHandlerT>::make_handler() {
    if (m_app) {
        return std::shared_ptr<base_handler_t>(new UserHandlerT(*m_app));
    } else {
        throw bad_factory_exception("Application has not been set.");
    }
}

class application_t {
    typedef std::map<std::string, std::shared_ptr<base_factory_t>>
            handlers_map;
public:
    virtual
    ~application_t() {
        // pass
    }

    virtual
    void
    initialize(const std::string& name,
               std::shared_ptr<service_manager_t> service_manager);

    virtual
    std::shared_ptr<base_handler_t>
    invoke(const std::string& event,
           std::shared_ptr<cocaine::api::stream_t> response);

    const std::string&
    name() const {
        return m_name;
    }

protected:
    virtual
    void
    on(const std::string& event,
       std::shared_ptr<base_factory_t> factory);

    template<class FactoryT>
    void
    on(const std::string& event,
       const FactoryT& factory);

    template<class UserHandlerT>
    void
    on(const std::string& event);

    virtual
    void
    on_unregistered(std::shared_ptr<base_factory_t> factory);

    template<class FactoryT>
    void
    on_unregistered(const FactoryT& factory);

    template<class UserHandlerT>
    void
    on_unregistered();

private:
    std::string m_name;
    handlers_map m_handlers;
    std::shared_ptr<base_factory_t> m_default_handler;
    std::shared_ptr<service_manager_t> m_service_manager;
};

template<class FactoryT>
void
application_t::on(const std::string& event,
                  const FactoryT& factory)
{
    FactoryT *new_factory = new FactoryT(factory);
    new_factory->set_application(dynamic_cast<typename FactoryT::application_type*>(this));
    this->on(event, std::shared_ptr<base_factory_t>(new_factory));
}

template<class UserHandlerT>
void
application_t::on(const std::string& event) {
    user_handler_factory<UserHandlerT> *new_factory = new user_handler_factory<UserHandlerT>;
    new_factory->set_application(dynamic_cast<typename UserHandlerT::application_type*>(this));
    this->on(event, std::shared_ptr<base_factory_t>(new_factory));
}

template<class FactoryT>
void
application_t::on_unregistered(const FactoryT& factory) {
    FactoryT *new_factory = new FactoryT(factory);
    new_factory->set_application(dynamic_cast<typename FactoryT::application_type*>(this));
    this->on_unregistered(std::shared_ptr<base_factory_t>(new_factory));
}

template<class UserHandlerT>
void
application_t::on_unregistered() {
    user_handler_factory<UserHandlerT> *new_factory = new user_handler_factory<UserHandlerT>;
    new_factory->set_application(dynamic_cast<typename UserHandlerT::application_type*>(this));
    this->on_unregistered(std::shared_ptr<base_factory_t>(new_factory));
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_APPLICATION_HPP
