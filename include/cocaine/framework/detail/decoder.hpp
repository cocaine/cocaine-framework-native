#pragma once

#include <memory>
#include <system_error>

#include <boost/none_t.hpp>

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/detail/forwards.hpp"

namespace cocaine {

namespace framework {

namespace detail {

struct decoder_t {
    typedef decoded_message message_type;

    size_t decode(const char* data, size_t size, message_type& message, std::error_code& ec);
};

}

}

}
