#ifndef COCAINE_FRAMEWORK_APPLICATION_HPP
#define COCAINE_FRAMEWORK_APPLICATION_HPP

#include <cocaine/framework/service.hpp>

#include <cocaine/api/stream.hpp>

#include <stdexcept>
#include <memory>
#include <string>
#include <map>
#include <boost/utility.hpp>

namespace cocaine { namespace framework {

class bad_factory_exception :
    public std::runtime_error
{
public:
    explicit bad_factory_exception(const std::string& what) :
        runtime_error(what)
    {
        // pass
    }
};

class handler_error :
    public std::runtime_error
{
public:
    explicit handler_error(const std::string& what) :
        runtime_error(what)
    {
        // pass
    }
};

class base_handler_t :
    private boost::noncopyable
{
    friend class application_t;
    friend class worker_t;

public:
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
    on_invoke() {
        // pass
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
    on_error(int code,
             const std::string& message) = 0;

    bool
    closed() const {
        return m_state == state_t::closed;
    }

private:
    void
    invoke(const std::string& event,
           std::shared_ptr<cocaine::api::stream_t> response)
    {
        m_event = event;
        m_response = response;
        m_state = state_t::opened;

        on_invoke();
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

    void
    error(int code,
          const std::string& message)
    {
        on_error(code, message);
    }

protected:
    std::string m_event;
    std::shared_ptr<cocaine::api::stream_t> m_response;

private:
    state_t m_state;
};

class base_factory_t {
public:
    virtual
    std::shared_ptr<base_handler_t>
    make_handler() = 0;
};

class application_t
{
    typedef std::map<std::string, std::shared_ptr<base_factory_t>>
            handlers_map;
public:
    application_t(const std::string& name,
                  std::shared_ptr<service_manager_t> service_manager);

    virtual
    ~application_t() {
        // pass
    }

    virtual
    std::shared_ptr<base_handler_t>
    invoke(const std::string& event,
           std::shared_ptr<cocaine::api::stream_t> response);

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
       std::shared_ptr<base_factory_t> factory);

    void
    on_unregistered(std::shared_ptr<base_factory_t> factory);

private:
    std::string m_name;
    std::shared_ptr<service_manager_t> m_service_manager;
    handlers_map m_handlers;
    std::shared_ptr<base_factory_t> m_default_handler;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_APPLICATION_HPP
