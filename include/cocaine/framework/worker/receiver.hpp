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

#include <memory>
#include <string>

#include <boost/optional/optional.hpp>

#include <cocaine/forwards.hpp>
#include <cocaine/hpack/header.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {
namespace framework {
namespace worker {

struct frame_t {
    std::string data;
    hpack::header_storage_t meta;
};

class receiver {
    hpack::header_storage_t headers;
    std::shared_ptr<basic_receiver_t<worker_session_t>> session;

public:
    /// \note this constructor is intentionally left implicit.
    receiver(const std::vector<hpack::header_t>& headers,
             std::shared_ptr<basic_receiver_t<worker_session_t>> session);

    /// Returns a const lvalue reference to headers that were passed with an invocation event.
    auto invocation_headers() const noexcept -> const hpack::header_storage_t&;

    template<typename R = std::string>
    auto recv() -> future<boost::optional<R>>;
};

template<>
auto receiver::recv<std::string>() -> future<boost::optional<std::string>>;

template<>
auto receiver::recv<frame_t>() -> future<boost::optional<frame_t>>;

} // namespace worker
} // namespace framework
} // namespace cocaine
