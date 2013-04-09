#ifndef COCAINE_FRAMEWORK_BASIC_APPLICATION_HPP
#define COCAINE_FRAMEWORK_BASIC_APPLICATION_HPP

#include <cocaine/framework/service.hpp>

#include <cocaine/framework/upstream.hpp>

#include <stdexcept>
#include <memory>
#include <string>
#include <map>

namespace cocaine { namespace framework {

class bad_factory_exception_t :
    public std::runtime_error
{
public:
    explicit bad_factory_exception_t(const std::string& what) :
        runtime_error(what)
    {
        // pass
    }
};

class handler_error_t :
    public std::runtime_error
{
public:
    explicit handler_error_t(const std::string& what) :
        runtime_error(what)
    {
        // pass
    }
};

class basic_handler_t {
    COCAINE_DECLARE_NONCOPYABLE(basic_handler_t)

    friend class basic_application_t;
    friend class worker_t;

public:

    basic_handler_t()
    {
        // pass
    }

    virtual
    ~basic_handler_t()
    {
        // pass
    }

    virtual
    void
    on_invoke() {
        // pass
    }

    virtual
    void
    on_chunk(const char *chunk,
             size_t size) = 0;

    virtual
    void
    on_error(int code,
             const std::string& message) = 0;

    virtual
    void
    on_close() {
        // pass
    }

    const std::string&
    event() const {
        return m_event;
    }

    std::shared_ptr<upstream_t>
    response() const {
        return m_response;
    }

private:
    void
    invoke(const std::string& event,
           std::shared_ptr<upstream_t> response)
    {
        m_event = event;
        m_response = response;

        on_invoke();
    }

private:
    std::string m_event;
    std::shared_ptr<upstream_t> m_response;
};

class basic_factory_t {
public:
    virtual
    std::shared_ptr<basic_handler_t>
    make_handler() = 0;
};

class basic_application_t
{
    typedef std::map<std::string, std::shared_ptr<basic_factory_t>>
            handlers_map;
public:
    basic_application_t(const std::string& name,
                        std::shared_ptr<service_manager_t> service_manager);

    virtual
    ~basic_application_t() {
        // pass
    }

    virtual
    std::shared_ptr<basic_handler_t>
    invoke(const std::string& event,
           std::shared_ptr<upstream_t> response);

    const std::string&
    name() const {
        return m_name;
    }

    std::shared_ptr<service_manager_t>
    service_manager() const {
        return m_service_manager;
    }

    void
    on(const std::string& event,
       std::shared_ptr<basic_factory_t> factory);

    void
    on_unregistered(std::shared_ptr<basic_factory_t> factory);

private:
    std::string m_name;
    std::shared_ptr<service_manager_t> m_service_manager;
    handlers_map m_handlers;
    std::shared_ptr<basic_factory_t> m_default_handler;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_BASIC_APPLICATION_HPP
