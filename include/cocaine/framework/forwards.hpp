/*
    Copyright (c) 2015 Evgeny Safronov <division494@gmail.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.
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

#pragma once

#include "cocaine/framework/util/future.hpp"

namespace cocaine {
    namespace framework {

        /// Workaround for compilers, that doesn't understand using keyword for template aliaces.
        template<typename T>
        struct task {
            typedef future<T> future_type;
            typedef typename std::add_lvalue_reference<future_type>::type future_move_type;

            typedef promise<T> promise_type;
            typedef typename std::add_lvalue_reference<promise_type>::type promise_move_type;
        };

        /// \internal
        struct event_loop_t;

        class scheduler_t;

        /// \internal
        class shared_state_t;

        class decoded_message;

        template<class Session>
        class basic_sender_t;

        template<class Session>
        class basic_receiver_t;

        template<class T, class Session>
        class sender;

        template<class T, class Session>
        class receiver;

        class basic_session_t;

        template<class BasicSession>
        class session;

        typedef session<basic_session_t> session_t;

        class basic_service_t;

        template<class T>
        class service;

        class service_manager_t;

        /// Worker side.
        class worker_session_t;

        struct options_t;
        class dispatch_t;
        class worker_t;
    } // namespace framework
} // namespace cocaine
