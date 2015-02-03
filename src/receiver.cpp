#include "cocaine/framework/receiver.hpp"

#include "cocaine/framework/session.hpp"

using namespace cocaine::framework;

basic_receiver_t::basic_receiver_t(std::uint64_t id, std::shared_ptr<basic_session_t> session) :
    id(id),
    session(session)
{}

basic_receiver_t::~basic_receiver_t() {
    session->revoke(id);
}

future_t<basic_receiver_t::result_type> basic_receiver_t::recv() {
    promise_t<basic_receiver_t::result_type> promise;
    auto future = promise.get_future();

    std::lock_guard<std::mutex> lock(mutex);
    if (broken) {
        COCAINE_ASSERT(queue.empty());

        promise.set_exception(std::system_error(broken.get()));
        return future;
    }

    if (queue.empty()) {
        pending.push(std::move(promise));
    } else {
        promise.set_value(std::move(queue.front()));
        queue.pop();
    }

    return future;
}

void basic_receiver_t::push(io::decoder_t::message_type&& message) {
    std::lock_guard<std::mutex> lock(mutex);
    COCAINE_ASSERT(!broken);

    if (pending.empty()) {
        queue.push(message);
    } else {
        auto promise = std::move(pending.front());
        promise.set_value(message);
        pending.pop();
    }
}

void basic_receiver_t::error(const std::error_code& ec) {
    std::lock_guard<std::mutex> lock(mutex);
    broken = ec;
    while (!pending.empty()) {
        pending.front().set_exception(std::system_error(ec));
        pending.pop();
    }
}
