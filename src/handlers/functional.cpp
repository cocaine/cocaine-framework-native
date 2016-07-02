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

#include <cocaine/framework/handlers/functional.hpp>

using namespace cocaine::framework;

function_handler_t::function_handler_t(function_handler_t::function_type_t f) :
    m_function(f)
{
    // pass
}

function_handler_t::~function_handler_t()
{
}

void
function_handler_t::on_chunk(const char *chunk,
                             size_t size)
{
    m_request.push_back(std::string(chunk, size));
}

void
function_handler_t::on_close() {
    m_function(event(), m_request, response());
}
