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

#include "cocaine/framework/detail/log.hpp"

#ifdef CF_USE_INTERNAL_LOGGING

#include <blackhole/formatter/string.hpp>
#include <blackhole/sink/stream.hpp>

namespace cocaine {

namespace framework {

namespace detail {

// R - resolver
// S - service
// s - session
// b - basic session
// [Ssb]C - connect
//      I - invoke
//      P - push
//      R - recv
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

std::string pop_context() {
    blackhole::scoped_attributes_t scoped(logger(), blackhole::attribute::set_t());
    const auto& attributes = scoped.attributes();
    auto it = std::find_if(attributes.begin(), attributes.end(), &match_context);

    if (it == attributes.end()) {
        return "";
    }

    std::string current = boost::get<std::string>(it->second.value);
    size_t pos = current.find_last_of("|");

    return current.substr(0, pos);
}

}

}

}

#endif
