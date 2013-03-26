#ifndef COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP
#define COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include <cocaine/framework/application.hpp>

namespace cocaine { namespace framework {

struct function_handler_t :
    public base_handler_t
{
    typedef std::function<std::string(const std::string&, const std::vector<std::string>&)>
            function_type;


    function_handler_t(function_type f);

    void
    on_chunk(const char *chunk,
             size_t size);

    void
    on_close();

private:
    function_type m_func;
    std::vector<std::string> m_input;
};

struct function_factory_t :
    public base_factory_t
{
    function_factory_t(function_handler_t::function_type f) :
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

inline
std::shared_ptr<base_factory_t>
function_factory(function_handler_t::function_type f) {
    return std::shared_ptr<base_factory_t>(
        new function_factory_t(f)
    );
}

template<class AppT>
struct method_factory :
    public base_factory_t
{
    friend class application_t;

    typedef AppT application_type;

    typedef std::function<std::string(AppT*, const std::string&, const std::vector<std::string>&)>
            method_type;

    method_factory(method_type f) :
        m_func(f),
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
    method_type m_func;
    application_type *m_app;
};

template<class AppT>
std::shared_ptr<base_handler_t>
method_factory<AppT>::make_handler() {
    if (m_app) {
        return std::shared_ptr<base_handler_t>(
            new function_handler_t(std::bind(m_func,
                                             m_app,
                                             std::placeholders::_1,
                                             std::placeholders::_2))
        );
    } else {
        throw bad_factory_exception("Application has not been set.");
    }
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP
