#include "cocaine/framework/detail/log.hpp"

#ifdef CF_USE_INTERNAL_LOGGING

#include <blackhole/formatter/string.hpp>
#include <blackhole/sink/stream.hpp>

namespace cocaine {

namespace framework {

namespace detail {

// S - service
// s - session
// C - connect
// I - invoke
// R - resolve with locator
// >> op - start asynchronous operation
// << op - finished asynchronous operation

void
map_severity(blackhole::aux::attachable_ostringstream& stream, const level_t& level) {
    typedef blackhole::aux::underlying_type<level_t>::type underlying_type;

    static std::array<const char*, 5> describe = {{ "D", "N", "I", "W", "E" }};

    const size_t value = static_cast<underlying_type>(level);

    if(value < describe.size()) {
        stream << describe[value];
    } else {
        stream << value;
    }
}

static blackhole::verbose_logger_t<level_t> create() {
    blackhole::verbose_logger_t<level_t> logger(level_t::debug);
    auto formatter = blackhole::aux::util::make_unique<
        blackhole::formatter::string_t
    >("[%(severity)s] [%(timestamp)s] [%(lwp)s]: %(context:[:] )s%(message)s");

    blackhole::mapping::value_t mapper;
    mapper.add<blackhole::keyword::tag::timestamp_t>("%H:%M:%S.%f");
    mapper.add<blackhole::keyword::tag::severity_t<level_t>>(&map_severity);
    formatter->set_mapper(mapper);

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

namespace {

bool match_context(const blackhole::attribute::pair_t& pair) {
    return pair.first == "context";
}

} // namespace

std::string merge_context(std::string context) {
    blackhole::scoped_attributes_t scoped(logger(), blackhole::attribute::set_t());
    const auto& attributes = scoped.attributes();
    auto it = std::find_if(attributes.begin(), attributes.end(), &match_context);

    if (it == attributes.end()) {
        return context;
    }

    return blackhole::utils::format("%s|%s", boost::get<std::string>(it->second.value), context);
}

}

}

}

#endif
