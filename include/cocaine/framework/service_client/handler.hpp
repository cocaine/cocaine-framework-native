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

#ifndef COCAINE_FRAMEWORK_SERVICE_HANDLER_HPP
#define COCAINE_FRAMEWORK_SERVICE_HANDLER_HPP

#include <cocaine/framework/service_client/error.hpp>
#include <cocaine/framework/generator.hpp>

#include <cocaine/rpc/message.hpp>
#include <cocaine/rpc/protocol.hpp>
#include <cocaine/traits/typelist.hpp>
#include <cocaine/traits/tuple.hpp>
#include <cocaine/traits/literal.hpp>
#include <cocaine/messages.hpp>

#include <memory>
#include <string>

namespace cocaine { namespace framework {

namespace detail { namespace service {

    struct service_handler_concept_t {
        virtual
        ~service_handler_concept_t() {
            // pass
        }

        virtual
        void
        handle_message(const cocaine::io::message_t&) = 0;

        virtual
        void
        error(std::exception_ptr e) = 0;
    };

    template<template<class...> class Wrapper, class... Args>
    struct wrapper_traits {
        typedef Wrapper<Args...> type;
    };

    template<template<class...> class Wrapper, class... Args>
    struct wrapper_traits<Wrapper, std::tuple<Args...>> {
        typedef Wrapper<Args...> type;
    };

    template<class Event, class ResultType = typename cocaine::io::event_traits<Event>::result_type>
    struct unpacker {
        typedef typename wrapper_traits<cocaine::framework::stream, ResultType>::type promise_type;
        typedef typename wrapper_traits<cocaine::framework::generator, ResultType>::type future_type;

        static
        inline
        void
        unpack(promise_type& p,
               const cocaine::io::literal& data)
        {
            msgpack::unpacked msg;
            msgpack::unpack(&msg, data.blob, data.size);

            ResultType r;
            cocaine::io::type_traits<ResultType>::unpack(msg.get(), r);
            p.write(std::move(r));
        }
    };

    template<class Event>
    struct unpacker<Event, cocaine::io::raw_t> {
        typedef typename wrapper_traits<cocaine::framework::stream, std::string>::type promise_type;
        typedef typename wrapper_traits<cocaine::framework::generator, std::string>::type future_type;

        static
        inline
        void
        unpack(promise_type& p,
               const cocaine::io::literal& data)
        {
            p.write(std::string(data.blob, data.size));
        }
    };

    template<class Event, class ResultType = typename cocaine::io::event_traits<Event>::result_type>
    struct message_handler {
        static
        inline
        void
        handle(typename unpacker<Event>::promise_type& p,
               const cocaine::io::message_t& message)
        {
            if (message.id() == io::event_traits<io::rpc::chunk>::id) {
                if (message.args().type != msgpack::type::ARRAY ||
                    message.args().via.array.size != 1 ||
                    message.args().via.array.ptr[0].type != msgpack::type::RAW)
                {
                    throw msgpack::type_error();
                } else {
                    cocaine::io::literal data = {
                        message.args().via.array.ptr[0].via.raw.ptr,
                        message.args().via.array.ptr[0].via.raw.size
                    };
                    unpacker<Event>::unpack(p, data);
                }
            } else if (message.id() == io::event_traits<io::rpc::error>::id) {
                int code;
                std::string msg;
                message.as<cocaine::io::rpc::error>(code, msg);
                p.error(cocaine::framework::make_exception_ptr(
                    service_error_t(std::error_code(code, service_response_category()), msg)
                ));
            }
        }
    };

    template<class Event>
    struct message_handler<Event, void> {
        static
        inline
        void
        handle(typename unpacker<Event>::promise_type& p,
               const cocaine::io::message_t& message)
        {
            if (message.id() == io::event_traits<io::rpc::error>::id) {
                int code;
                std::string msg;
                message.as<cocaine::io::rpc::error>(code, msg);
                p.error(cocaine::framework::make_exception_ptr(
                    service_error_t(std::error_code(code, service_response_category()), msg)
                ));
            }
        }
    };

    template<class Event>
    class service_handler :
        public service_handler_concept_t
    {
        COCAINE_DECLARE_NONCOPYABLE(service_handler)

    public:
        typedef typename unpacker<Event>::future_type
                future_type;

        typedef typename unpacker<Event>::promise_type
                promise_type;

        service_handler()
        {
            // pass
        }

        service_handler(service_handler&& other) :
            m_promise(std::move(other.m_promise))
        {
            // pass
        }

        future_type
        get_future() {
            return m_promise.get_generator();
        }

        void
        handle_message(const cocaine::io::message_t& message) {
            message_handler<Event>::handle(m_promise, message);
        }

        void
        error(std::exception_ptr e) {
            m_promise.error(e);
        }

    protected:
        promise_type m_promise;
    };

}} // namespace detail::service

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICE_HANDLER_HPP
