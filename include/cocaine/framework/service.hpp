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

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/receiver.hpp"
#include "cocaine/framework/service.inl.hpp"
#include "cocaine/framework/session.hpp"
#include "cocaine/framework/trace.hpp"

#include <cocaine/idl/logging.hpp>
#include <cocaine/trace/trace_logger.hpp>

/// This module provides access to the client part of the Framework.
///
/// \unstable because it needs some user experience.

namespace cocaine { namespace framework {

/// The basic service class represents an untyped Cocaine service.
///
/// You are restricted to create instances of this class directly, use \sa service_manager_t for
/// this purposes.
class basic_service_t {
public:
    typedef session_t::native_handle_type native_handle_type;
    typedef std::vector<session_t::endpoint_type> endpoints_t;

private:
    class impl;
    std::unique_ptr<impl> d;
    std::shared_ptr<session_t> session;
    scheduler_t& scheduler;

public:
    /// Constructs an instance of the service.
    ///
    /// \param name a service's name, which is used by the Locator to resolve this service.
    /// \param version a protocol version number.
    /// \param locations list of the Locator endpoints which is usually well-known.
    /// \param scheduler an object which incapsulates an IO event loop inside itself.
    basic_service_t(std::string name, uint version, endpoints_t locations, scheduler_t& scheduler);

    /// Constructs an instance of the service via moving already existing instance.
    basic_service_t(basic_service_t&& other);

    ~basic_service_t();

    /// Returns the name of this service given at the construction.
    const std::string&
    name() const noexcept;

    /// Returns the protocol version number of this service given at the construction.
    uint
    version() const noexcept;

    /// Tries to connect to the service through the Locator.
    ///
    /// \returns a future which is set after the connection is established.
    future<void>
    connect();

    boost::optional<session_t::endpoint_type>
    endpoint() const;

    /// Get the native socket representation.
    ///
    /// This function may be used to obtain the underlying representation of the socket. This is
    /// intended to allow access to native socket functionality that is not otherwise provided.
    native_handle_type
    native_handle() const;

    template<class Event, class... Args>
    typename task<typename invocation_result<Event>::type>::future_type
    invoke(Args&&... args) {
        namespace ph = std::placeholders;

        trace::context_holder holder("SI");

        return connect()
            .then(scheduler, trace::wrap(trace_t::bind(&basic_service_t::on_connect<Event, Args...>, ph::_1, session, std::forward<Args>(args)...)))
            .then(scheduler, trace::wrap(trace_t::bind(&basic_service_t::on_invoke<Event>, ph::_1)));
    }

private:
    template<class Event, class... Args>
    static
    typename task<channel<Event>>::future_type
    on_connect(task<void>::future_move_type future, std::shared_ptr<session_t> session, Args&... args) {
        future.get();
        // Between these calls no one can guarantee, that the connection won't be broken. In this
        // case you will get a system error after either write or read attempt.
        return session->invoke<Event>(std::forward<Args>(args)...);
    }

    template<class Event>
    static
    typename task<typename invocation_result<Event>::type>::future_type
    on_invoke(typename task<channel<Event>>::future_move_type future) {
        return invocation_result<Event>::apply(future.get());
    }
};

/// The service class represents a typed Cocaine service.
///
/// You are restricted to create instances of this class directly, use \sa service_manager_t for
/// this purposes.
template<class T>
class service : public basic_service_t {
public:
    service(std::string name, endpoints_t locations, scheduler_t& scheduler) :
        basic_service_t(std::move(name), io::protocol<T>::version::value, std::move(locations), scheduler)
    {}
};

}} // namespace cocaine::framework

