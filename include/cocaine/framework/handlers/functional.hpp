#ifndef COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP
#define COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP

#include <cocaine/framework/basic_application.hpp>

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace cocaine { namespace framework {

class event_t {
public:
    event_t(const std::string& name, std::shared_ptr<upstream_t> upstream) :
        m_name(name),
        m_response(upstream)
    {
        // pass
    }

    const std::string&
    name() const {
        return m_name;
    }

    std::shared_ptr<upstream_t>
    response() const {
        return m_response;
    }
private:
    std::string m_name;
    std::shared_ptr<upstream_t> m_response;
};

class function_handler_t :
    public basic_handler_t
{
public:
    typedef std::function<void(const event_t&, const std::vector<std::string>&)>
            function_type_t;

    function_handler_t(function_type_t f);

    void
    on_chunk(const char *chunk,
             size_t size);

    void
    on_close();

private:
    function_type_t m_func;
    std::vector<std::string> m_input;
};

class function_factory_t :
    public basic_factory_t
{
public:
    function_factory_t(function_handler_t::function_type_t f) :
        m_func(f)
    {
        // pass
    }

    std::shared_ptr<basic_handler_t>
    make_handler()
    {
        return std::shared_ptr<basic_handler_t>(new function_handler_t(m_func));
    }

private:
    function_handler_t::function_type_t m_func;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP
