/*
Copyright (c) 2013 Andrey Goryachev <andrey.goryachev@gmail.com>
Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

This file is part of Cocaine.

Cocaine is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

Cocaine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP
#define COCAINE_FRAMEWORK_HANDLERS_FUNCTIONAL_HPP

#include <cocaine/framework/handler.hpp>

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
