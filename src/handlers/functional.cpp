#include <cocaine/framework/handlers/functional.hpp>

using namespace cocaine::framework;

function_handler_t::function_handler_t(function_handler_t::function_type_t f) :
    m_func(f)
{
    // pass
}

void
function_handler_t::on_chunk(const char *chunk,
                             size_t size)
{
    m_input.push_back(std::string(chunk, size));
}

void
function_handler_t::on_close() {
    std::string result = m_func(event(), m_input);
    response()->write(result.data(), result.size());
    response()->close();
}

void
function_handler_t::on_error(int code,
                             const std::string& message)
{
    // pass
}
