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

#ifndef COCAINE_FRAMEWORK_FUTURE_VARIANT_HPP
#define COCAINE_FRAMEWORK_FUTURE_VARIANT_HPP

#include <cocaine/framework/common.hpp>

#include <type_traits>
#include <typeinfo>
#include <utility>
#include <boost/type_traits.hpp>

namespace cocaine { namespace framework { namespace detail { namespace future {

template<template<class> class F, class... Args>
struct max_value;

template<template<class> class F, class Arg>
struct max_value<F, Arg> :
    public std::integral_constant<size_t, F<Arg>::value>
{
    // pass
};

template<template<class > class F, class Arg, class... Args>
struct max_value<F, Arg, Args...> :
    public std::conditional<
        (F<Arg>::value > max_value<F, Args...>::value),
        std::integral_constant<size_t, F<Arg>::value>,
        std::integral_constant<size_t, max_value<F, Args...>::value>
    >::type
{
    // pass
};

template<class T>
struct size_of:
    public std::integral_constant<size_t, sizeof(T)>
{
    // pass
};

template<unsigned int N, class... Args>
struct get_nth;

template<class Arg, class... Args>
struct get_nth<0, Arg, Args...> {
    typedef Arg
            type;
};

template<unsigned int N, class Arg, class... Args>
struct get_nth<N, Arg, Args...> {
    typedef typename get_nth<N - 1, Args...>::type
            type;
};

template<unsigned int N, unsigned int Last>
struct visiting_impl {
    template<class Visitor, class Variant>
    static
    inline
    typename Visitor::result_type
    apply(Visitor&& vis, Variant&& var) {
        if (var.template is<N>()) {
            return vis.template visit<N>(var.template as<N>());
        } else {
            return visiting_impl<N + 1, Last>::apply(std::forward<Visitor>(vis),
                                                     std::forward<Variant>(var));
        }
    }
};

template<unsigned int Last>
struct visiting_impl<Last, Last> {
    template<class Visitor, class Variant>
    static
    inline
    typename Visitor::result_type
    apply(Visitor&& vis, Variant&& var) {
        if (var.template is<Last>()) {
            return vis.template visit<Last>(var.template as<Last>());
        } else {
            throw std::bad_cast();
        }
    }
};

struct clean_visitor {
    typedef void result_type;

    template<unsigned int, class T>
    void
    visit(T& v) const {
        v.~T();
    }
};

template<class... Args>
class variant {
    COCAINE_DECLARE_NONCOPYABLE(variant)

    template<unsigned int N, unsigned int Last> friend struct visiting_impl;

    struct move_visitor {
        typedef void result_type;

        move_visitor(variant& v) :
            m_variant(v)
        {
            // pass
        }

        template<unsigned int N, class T>
        void
        visit(T& v) {
            m_variant.assign<N>(std::move(v));
        }

    private:
        variant& m_variant;
    };

    friend struct move_visitor;

public:
    variant() :
        m_which(0)
    {
        // pass
    }

    variant(variant&& other) :
        m_which(0)
    {
        other.apply(move_visitor(*this));
    }

    ~variant() {
        clean();
    }

    variant&
    operator=(variant&& other) {
        clean();
        other.apply(move_visitor(*this));
        return *this;
    }

    template<class Visitor>
    typename Visitor::result_type
    apply(Visitor&& v) {
        return visiting_impl<0, sizeof...(Args) - 1>::apply(std::forward<Visitor>(v), *this);
    }

    template<class Visitor>
    typename Visitor::result_type
    apply(Visitor&& v) const {
        return visiting_impl<0, sizeof...(Args) - 1>::apply(std::forward<Visitor>(v), *this);
    }

    template<unsigned int N>
    bool
    is() const {
        return N + 1 == m_which;
    }

    bool
    empty() const {
        return m_which == 0;
    }

    unsigned int
    which() const {
        return m_which - 1;
    }

    template<unsigned int N>
    const typename get_nth<N, Args...>::type&
    get() const {
        if (!is<N>()) {
            throw std::bad_cast();
        }
        return as<N>();
    }

    template<unsigned int N>
    typename get_nth<N, Args...>::type&
    get() {
        if (!is<N>()) {
            throw std::bad_cast();
        }
        return as<N>();
    }

    template<unsigned int N, class... Args2>
    void
    set(Args2&&... args) {
        clean();
        assign<N, Args2...>(std::forward<Args2>(args)...);
    }

    void
    clean() {
        if (!empty()) {
            apply(clean_visitor());
            m_which = 0;
        }
    }

protected:
    template<unsigned int N>
    typename get_nth<N, Args...>::type&
    as() {
        return *static_cast<typename get_nth<N, Args...>::type *>(static_cast<void*>(&m_storage));
    }

    template<unsigned int N>
    const typename get_nth<N, Args...>::type&
    as() const {
        return *static_cast<const typename get_nth<N, Args...>::type *>(static_cast<const void*>(&m_storage));
    }

    template<unsigned int N, class... Args2>
    void
    assign(Args2&&... args) {
        typedef typename get_nth<N, Args...>::type type;
        new(&m_storage) type(std::forward<Args2>(args)...);
        m_which = N + 1;
    }

private:
    unsigned int m_which;

    typedef typename boost::aligned_storage<
            max_value<size_of, Args...>::value,
            max_value<boost::alignment_of, Args...>::value
        >::type
    storage_type;

    storage_type m_storage;
};

}}}} // namespace cocaine::framework::detail::future

#endif // COCAINE_FRAMEWORK_FUTURE_VARIANT_HPP
