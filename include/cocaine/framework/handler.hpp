#ifndef COCAINE_FRAMEWORK_BASIC_APPLICATION_HPP
#define COCAINE_FRAMEWORK_BASIC_APPLICATION_HPP

#include <cocaine/framework/service.hpp>

#include <cocaine/framework/upstream.hpp>

#include <stdexcept>
#include <memory>
#include <string>
#include <map>

namespace cocaine { namespace framework {

class factory_error_t :
    public std::runtime_error
{
public:
    explicit factory_error_t(const std::string& what) :
        runtime_error(what)
    {
        // pass
    }
};

class basic_handler_t {
    COCAINE_DECLARE_NONCOPYABLE(basic_handler_t)

    friend class dispatch_t;

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
    on_error(int /* code */,
             const std::string& /* message */)
    {
        // pass
    }

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


template<class App>
struct handler :
    public basic_handler_t
{
    typedef App application_type;

    handler(application_type *app) :
        m_app(app)
    {
        // pass
    }

protected:
    application_type*
    app() const {
        return m_app;
    }

private:
    application_type *m_app;
};

template<class Handler>
class handler_factory :
    public basic_factory_t
{
    typedef typename Handler::application_type application_type;

public:
    handler_factory(application_type *app) :
        m_app(app)
    {
        // pass
    }

    std::shared_ptr<basic_handler_t>
    make_handler();

private:
    application_type *m_app;
};

template<class Handler>
std::shared_ptr<basic_handler_t>
handler_factory<Handler>::make_handler() {
    return std::make_shared<Handler>(m_app);
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_BASIC_APPLICATION_HPP
