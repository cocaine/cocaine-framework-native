#include "cocaine/framework/detail/log.hpp"

#ifdef CF_USE_INTERNAL_LOGGING

#include <blackhole/formatter/string.hpp>
#include <blackhole/sink/stream.hpp>

namespace cocaine {

namespace framework {

namespace detail {

blackhole::verbose_logger_t<level_t> create() {
    blackhole::verbose_logger_t<level_t> logger(level_t::debug);
    auto formatter = blackhole::aux::util::make_unique<
        blackhole::formatter::string_t
    >("[%(timestamp)s] [%(tid)s]: %(context:[:] )s%(message)s");

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

logger_type& logger() {
    static logger_type log = create();
    return log;
}

std::string merge_context(std::string context) {
    blackhole::scoped_attributes_t scoped(logger(), blackhole::attribute::set_t());
    const auto& attributes = scoped.attributes();
    auto it = std::find_if(attributes.begin(), attributes.end(), [](const blackhole::attribute::pair_t& pair){
        return pair.first == "context";
    });

    if (it == attributes.end()) {
        return context;
    }

    return blackhole::utils::format("%s|%s", boost::get<std::string>(it->second.value), context);
}

}

}

}

#endif
