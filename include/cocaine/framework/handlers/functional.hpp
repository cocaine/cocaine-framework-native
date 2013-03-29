#ifndef COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP
#define COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP

#include <cocaine/framework/application.hpp>

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace cocaine { namespace framework {

class function_handler_t :
    public base_handler_t
{
public:
    typedef std::function<std::string(const std::string&, const std::vector<std::string>&)>
            function_type;

    function_handler_t(function_type f);

    void
    on_chunk(const char *chunk,
             size_t size);

    void
    on_close();

    void
    on_error(int code,
             const std::string& message);

private:
    function_type m_func;
    std::vector<std::string> m_input;
};

class function_factory :
    public base_factory_t
{
public:
    function_factory(function_handler_t::function_type f) :
        m_func(f)
    {
        // pass
    }

    std::shared_ptr<base_handler_t>
    make_handler()
    {
        return std::shared_ptr<base_handler_t>(new function_handler_t(m_func));
    }

private:
    function_handler_t::function_type m_func;
};

template<class Class>
class method_factory :
    public base_factory_t
{
public:
    typedef std::function<std::string(Class*, const std::string&, const std::vector<std::string>&)>
            method_type;

    method_factory(Class *object, method_type method) :
        m_object(object),
        m_method(method)
    {
        // pass
    }

    std::shared_ptr<base_handler_t>
    make_handler();

private:
    Class *m_object;
    method_type m_method;
};

template<class AppT>
std::shared_ptr<base_handler_t>
method_factory<AppT>::make_handler() {
    if (m_object) {
        return std::shared_ptr<base_handler_t>(
            new function_handler_t(std::bind(m_method,
                                             m_object,
                                             std::placeholders::_1,
                                             std::placeholders::_2))
        );
    } else {
        throw bad_factory_exception("Object has not been set.");
    }
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP
