#include <cocaine/framework/future_error.hpp>

namespace {

    struct my_future_category_impl :
        public std::error_category
    {
        const char*
        name() const throw() {
            return "future";
        }

        std::string
        message(int error_value) const throw() {
            switch (error_value) {
                case static_cast<int>(cocaine::framework::future_errc::broken_promise):
                    return "The promise is broken";
                case static_cast<int>(cocaine::framework::future_errc::future_already_retrieved):
                    return "The future is already retrieved";
                case static_cast<int>(cocaine::framework::future_errc::promise_already_satisfied):
                    return "The promise is already satisfied";
                case static_cast<int>(cocaine::framework::future_errc::no_state):
                    return "No associated state";
                case static_cast<int>(cocaine::framework::future_errc::stream_closed):
                    return "The stream is closed";
                default:
                    return "Unknown future error o_O";
            }
        }
    };

    my_future_category_impl category;

} // namespace

const std::error_category&
cocaine::framework::future_category() {
    return category;
}

std::error_code
cocaine::framework::make_error_code(cocaine::framework::future_errc e) {
    return std::error_code(static_cast<int>(e), cocaine::framework::future_category());
}

std::error_condition
cocaine::framework::make_error_condition(cocaine::framework::future_errc e) {
    return std::error_condition(static_cast<int>(e), cocaine::framework::future_category());
}
