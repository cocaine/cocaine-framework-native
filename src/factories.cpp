#include <cocaine/framework/factories.hpp>

using namespace cocaine::framework;

function_handler_t::function_handler_t(function_handler_t::function_type f) :
    m_func(f)
{
    // pass
}

void
function_handler_t::write(const char *chunk,
                          size_t size)
{
    if (!closed()) {
        m_input.push_back(std::string(chunk, size));
    }
}

void
function_handler_t::close() {
    std::string result = m_func(m_event, m_input);
    m_response->write(result.data(), result.size());
    m_response->close();
    base_handler_t::close();
}
