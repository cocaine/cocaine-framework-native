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

#include "cocaine/framework/detail/shared_state.hpp"

using namespace cocaine::framework;

void shared_state_t::put(value_type&& message) {
    std::unique_lock<std::mutex> lock(mutex);

    BOOST_ASSERT(!broken);

    if (await.empty()) {
        queue.push(std::move(message));
    } else {
        auto promise = await.front();
        await.pop();
        lock.unlock();

        promise.set_value(std::move(message));
    }
}

void shared_state_t::put(const std::error_code& ec) {
    std::unique_lock<std::mutex> lock(mutex);

    BOOST_ASSERT(!broken);

    broken = ec;
    std::queue<typename task<value_type>::promise_type> await(std::move(this->await));
    lock.unlock();

    while (!await.empty()) {
        await.front().set_exception(std::system_error(ec));
        await.pop();
    }
}

auto shared_state_t::get() -> typename task<value_type>::future_type {
    std::lock_guard<std::mutex> lock(mutex);

    if (broken) {
        return make_ready_future<value_type>::error(std::system_error(broken.get()));
    }

    if (queue.empty()) {
        await.push(typename task<value_type>::promise_type());
        return await.back().get_future();
    }

    auto future = make_ready_future<value_type>::value(std::move(queue.front()));
    queue.pop();
    return future;
}
