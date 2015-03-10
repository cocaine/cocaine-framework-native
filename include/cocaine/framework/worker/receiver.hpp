#pragma once

#include <memory>
#include <string>

#include <boost/optional/optional.hpp>

#include <cocaine/forwards.hpp>

#include "cocaine/framework/forwards.hpp"

namespace cocaine {

namespace framework {

namespace worker {

class receiver {
    std::shared_ptr<basic_receiver_t<worker_session_t>> session;

public:
    /*!
     * \note this constructor is intentionally left implicit.
     */
    receiver(std::shared_ptr<basic_receiver_t<worker_session_t>> session);

    auto recv() -> typename task<boost::optional<std::string>>::future_type;
};

} // namespace worker

} // namespace framework

} // namespace cocaine
