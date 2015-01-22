#include <blackhole/formatter/string.hpp>
#include <blackhole/sink/stream.hpp>

#include "cocaine/framework/detail/log.hpp"

blackhole::verbose_logger_t<cocaine::framework::detail::level_t> cocaine::framework::detail::create() {
    blackhole::verbose_logger_t<cocaine::framework::detail::level_t> logger(cocaine::framework::detail::level_t::debug);
    auto formatter = blackhole::aux::util::make_unique<
        blackhole::formatter::string_t
    >("[%(timestamp)s] [%(tid)s]: %(message)s");

    auto sink = blackhole::aux::util::make_unique<
        blackhole::sink::stream_t
    >(blackhole::sink::stream_t::output_t::stdout);

    auto frontend = blackhole::aux::util::make_unique<
        blackhole::frontend_t<
            blackhole::formatter::string_t,
            blackhole::sink::stream_t
        >
    >(std::move(formatter), std::move(sink));

    logger.add_frontend(std::move(frontend));
    return logger;
}
