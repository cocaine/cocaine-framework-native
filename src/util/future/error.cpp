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

#include "cocaine/framework/util/future/error.hpp"

namespace {

    struct my_future_category_impl :
        public std::error_category
    {
        const char*
        name() const throw() {
            return "future";
        }

        std::string
        message(int error_value) const throw() {
            switch (error_value) {
                case static_cast<int>(cocaine::framework::future_errc::broken_promise):
                    return "The promise is broken";
                case static_cast<int>(cocaine::framework::future_errc::future_already_retrieved):
                    return "The future is already retrieved";
                case static_cast<int>(cocaine::framework::future_errc::promise_already_satisfied):
                    return "The promise is already satisfied";
                case static_cast<int>(cocaine::framework::future_errc::no_state):
                    return "No associated state";
                case static_cast<int>(cocaine::framework::future_errc::stream_closed):
                    return "The stream is closed";
                default:
                    return "Unknown future error o_O";
            }
        }
    };

    my_future_category_impl category;

} // namespace

const std::error_category&
cocaine::framework::future_category() {
    return category;
}

std::error_code
cocaine::framework::make_error_code(cocaine::framework::future_errc e) {
    return std::error_code(static_cast<int>(e), cocaine::framework::future_category());
}

std::error_condition
cocaine::framework::make_error_condition(cocaine::framework::future_errc e) {
    return std::error_condition(static_cast<int>(e), cocaine::framework::future_category());
}
