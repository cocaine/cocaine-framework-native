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

#ifndef COCAINE_FRAMEWORK_BASIC_APPLICATION_HPP
#define COCAINE_FRAMEWORK_BASIC_APPLICATION_HPP

#include <cocaine/framework/service.hpp>

#include <cocaine/framework/upstream.hpp>

#include <stdexcept>
#include <memory>
#include <string>
#include <map>

namespace cocaine { namespace framework {

class factory_error_t :
    public std::runtime_error
{
public:
    explicit factory_error_t(const std::string& what) :
        runtime_error(what)
    {
        // pass
    }
};

class basic_handler_t {
    COCAINE_DECLARE_NONCOPYABLE(basic_handler_t)

    friend class dispatch_t;

public:

    basic_handler_t()
    {
        // pass
    }

    virtual
    ~basic_handler_t()
    {
        // pass
    }

    virtual
    void
    on_invoke() {
        // pass
    }

    virtual
    void
    on_chunk(const char *chunk,
             size_t size) = 0;

    virtual
    void
    on_error(int /* code */,
             const std::string& /* message */)
    {
        // pass
    }

    virtual
    void
    on_close() {
        // pass
    }

    const std::string&
    event() const {
        return m_event;
    }

    std::shared_ptr<upstream_t>
    response() const {
        return m_response;
    }

private:
    void
    invoke(const std::string& event,
           std::shared_ptr<upstream_t> response)
    {
        m_event = event;
        m_response = response;

        on_invoke();
    }

private:
    std::string m_event;
    std::shared_ptr<upstream_t> m_response;
};

class basic_factory_t {
public:
    virtual
    std::shared_ptr<basic_handler_t>
    make_handler() = 0;
};


template<class App>
struct handler :
    public basic_handler_t
{
    typedef App application_type;

    handler(application_type &parent) :
        m_parent(parent)
    {
        // pass
    }

protected:
    application_type&
    parent() const {
        return m_parent;
    }

private:
    application_type &m_parent;
};

template<class Handler>
class handler_factory :
    public basic_factory_t
{
    typedef typename Handler::application_type application_type;

public:
    handler_factory(application_type &parent) :
        m_parent(parent)
    {
        // pass
    }

    std::shared_ptr<basic_handler_t>
    make_handler();

private:
    application_type &m_parent;
};

template<class Handler>
std::shared_ptr<basic_handler_t>
handler_factory<Handler>::make_handler() {
    return std::make_shared<Handler>(m_parent);
}

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_BASIC_APPLICATION_HPP
