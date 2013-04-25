#ifndef COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP
#define COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP

#include <cocaine/framework/basic_application.hpp>

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace cocaine { namespace framework {

typedef std::shared_ptr<upstream_t> response_ptr;

class function_handler_t :
    public basic_handler_t
{
public:
    typedef std::function<void(const std::string&,
                               const std::vector<std::string>&,
                               response_ptr)>
            function_type_t;

    function_handler_t(function_type_t f);

    void
    on_chunk(const char *chunk,
             size_t size);

    void
    on_close();

private:
    function_type_t m_function;
    std::vector<std::string> m_request;
};

class function_factory_t :
    public basic_factory_t
{
public:
    function_factory_t(function_handler_t::function_type_t f) :
        m_function(f)
    {
        // pass
    }

    std::shared_ptr<basic_handler_t>
    make_handler()
    {
        return std::shared_ptr<basic_handler_t>(new function_handler_t(m_function));
    }

private:
    function_handler_t::function_type_t m_function;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP
