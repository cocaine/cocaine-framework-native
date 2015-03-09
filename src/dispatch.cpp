/*
Copyright (c) 2013 Andrey Goryachev <andrey.goryachev@gmail.com>
Copyright (c) 2013 Andrey Sibiryov <me@kobology.ru>
Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include <cocaine/framework/dispatch.hpp>

#include <cocaine/messages.hpp>

#include <csignal>
#include <stdexcept>
#include <iostream>
#include <boost/program_options.hpp>

namespace cocaine { namespace framework { namespace detail {
class dispatch_upstream_t:
    public upstream_t
{
    enum class state_t: int {
        open,
        closed
    };
public:
    dispatch_upstream_t(uint64_t id,
                        dispatch_t * const dispatch):
        m_id(id),
        m_dispatch(dispatch),
        m_state(state_t::open)
    {
        // pass
    }

    ~dispatch_upstream_t() {
        if (!closed()) {
            close();
        }
    }

    void
    write(const char * chunk,
          size_t size)
    {
        std::lock_guard<std::mutex> lock(m_closed_lock);
        if (m_state == state_t::closed) {
            throw std::runtime_error("The stream has been closed");
        } else {
            send<io::rpc::chunk>(std::string(chunk, size));
        }
    }

    void
    error(int code,
          const std::string& message)
    {
        std::lock_guard<std::mutex> lock(m_closed_lock);
        if (m_state == state_t::closed) {
            throw std::runtime_error("The stream has been closed");
        } else {
            m_state = state_t::closed;
            send<io::rpc::error>(code, message);
            send<io::rpc::choke>();
        }
    }

    void
    close() {
        std::lock_guard<std::mutex> lock(m_closed_lock);
        if (m_state == state_t::closed) {
            throw std::runtime_error("The stream has been closed.");
        } else {
            m_state = state_t::closed;
            send<io::rpc::choke>();
        }
    }

    bool
    closed() const {
        return m_state == state_t::closed;
    }

private:
    template<class Event, typename... Args>
    void
    send(Args&&... args) {
        m_dispatch->send<Event>(m_id, std::forward<Args>(args)...);
    }

private:
    const uint64_t m_id;
    dispatch_t * const m_dispatch;
    state_t m_state;

    std::mutex m_closed_lock;
};

}}} // namespace cocaine::framework::detail

using namespace cocaine;
using namespace cocaine::framework;

namespace {

void
on_socket_error(const std::error_code& code) {
    throw std::system_error(code);
}

} // namespace

dispatch_t::dispatch_t(const std::string& name,
                       const std::string& uuid,
                       const std::string& endpoint,
                       const service_manager_t::endpoint_t& locator_endpoint):
    m_id(uuid),
    m_heartbeat_timer(m_ioservice.native()),
    m_disown_timer(m_ioservice.native())
{
    auto socket = std::make_shared<io::socket<io::local>>(io::local::endpoint(endpoint));
    m_channel.reset(new io::channel<io::socket<io::local>>(m_ioservice, socket));
    m_channel->rd->bind(std::bind(&dispatch_t::on_message, this, std::placeholders::_1),
                        &on_socket_error);
    m_channel->wr->bind(&on_socket_error);

    // Greet the engine!
    send<io::rpc::handshake>(0ul, m_id);

    m_heartbeat_timer.set<dispatch_t, &dispatch_t::on_heartbeat>(this);
    m_heartbeat_timer.start(0.0f, 5.0f);

    m_disown_timer.set<dispatch_t, &dispatch_t::on_disown>(this);

    // Set the lowest priority for the disown timer.
    m_disown_timer.priority = EV_MINPRI;

    m_service_manager = service_manager_t::create(locator_endpoint, "app/" + name);
}

void
dispatch_t::on_message(const io::message_t& message) {
    COCAINE_LOG_DEBUG(service_manager()->get_system_logger(),
                      "worker %s received type %d message",
                      m_id, message.id());

    switch(message.id()) {
        case io::event_traits<io::rpc::heartbeat>::id: {
            m_disown_timer.stop();
            break;
        }
        case io::event_traits<io::rpc::invoke>::id: {
            std::string event;
            message.as<io::rpc::invoke>(event);

            COCAINE_LOG_DEBUG(service_manager()->get_system_logger(),
                              "worker %s invoking session %d with event '%s'",
                              m_id,
                              message.band(),
                              event);

            std::shared_ptr<detail::dispatch_upstream_t> upstream(
                std::make_shared<detail::dispatch_upstream_t>(message.band(), this)
            );

            try {
                io_pair_t session = {upstream, invoke(event, upstream)};
                m_sessions.insert(std::make_pair(message.band(), session));
            } catch(const std::exception& e) {
                upstream->error(invocation_error, e.what());
            } catch(...) {
                upstream->error(invocation_error, "unexpected exception");
            }

            break;
        }
        case io::event_traits<io::rpc::chunk>::id: {
            std::string chunk;
            message.as<io::rpc::chunk>(chunk);

            stream_map_t::iterator it = m_sessions.find(message.band());

            // NOTE: This may be a chunk for a failed invocation, in which case there
            // will be no active stream, so drop the message.
            if(it != m_sessions.end()) {
                try {
                    it->second.handler->on_chunk(chunk.data(), chunk.size());
                } catch(const std::exception& e) {
                    it->second.upstream->error(invocation_error, e.what());
                    m_sessions.erase(it);
                } catch(...) {
                    it->second.upstream->error(invocation_error, "unexpected exception");
                    m_sessions.erase(it);
                }
            }

            break;
        }
        case io::event_traits<io::rpc::choke>::id: {
            stream_map_t::iterator it = m_sessions.find(message.band());

            // NOTE: This may be a choke for a failed invocation, in which case there
            // will be no active stream, so drop the message.
            if(it != m_sessions.end()) {
                try {
                    it->second.handler->on_close();
                } catch(const std::exception& e) {
                    it->second.upstream->error(invocation_error, e.what());
                } catch(...) {
                    it->second.upstream->error(invocation_error, "unexpected exception");
                }

                m_sessions.erase(it);
            }

            break;
        }
        case io::event_traits<io::rpc::error>::id: {
            int ec;
            std::string error_message;
            message.as<io::rpc::error>(ec, error_message);

            stream_map_t::iterator it = m_sessions.find(message.band());

            // NOTE: This may be a chunk for a failed invocation, in which case there
            // will be no active stream, so drop the message.
            if(it != m_sessions.end()) {
                try {
                    it->second.handler->on_error(ec, error_message);
                } catch(const std::exception& e) {
                    it->second.upstream->error(invocation_error, e.what());
                    m_sessions.erase(it);
                } catch(...) {
                    it->second.upstream->error(invocation_error, "unexpected exception");
                    m_sessions.erase(it);
                }
            }

            break;
        }
        case io::event_traits<io::rpc::terminate>::id: {
            terminate(io::rpc::terminate::normal, "per request");
            break;
        }
        default: {
            COCAINE_LOG_WARNING(service_manager()->get_system_logger(),
                                "worker %s dropping unknown type %d message",
                                m_id,
                                message.id());
        }
    }
}

void
dispatch_t::on_heartbeat(ev::timer&,
                         int)
{
    send<io::rpc::heartbeat>(0ul);
    m_disown_timer.start(60.0f);
}

void
dispatch_t::on_disown(ev::timer&,
                      int)
{
    COCAINE_LOG_ERROR(service_manager()->get_system_logger(),
                      "worker %s has lost the controlling engine",
                      m_id);
    m_ioservice.native().unloop(ev::ALL);
}

void
dispatch_t::terminate(int code,
                      const std::string& reason)
{
    send<io::rpc::terminate>(0ul, static_cast<io::rpc::terminate::code>(code), reason);
    m_ioservice.native().unloop(ev::ALL);
}

void
dispatch_t::run() {
    m_ioservice.run();
    m_service_manager.reset();
    m_sessions.clear();
}

void
dispatch_t::on(const std::string& event,
               std::shared_ptr<basic_factory_t> factory)
{
    m_handlers[event] = factory;
}

std::shared_ptr<basic_handler_t>
dispatch_t::invoke(const std::string& event,
                   std::shared_ptr<upstream_t> response)
{
    auto it = m_handlers.find(event);

    if (it != m_handlers.end()) {
        std::shared_ptr<basic_handler_t> new_handler = it->second->make_handler();
        new_handler->invoke(event, response);
        return new_handler;
    } else {
        throw std::runtime_error(
            cocaine::format("Unrecognized event '%s' has been accepted", event)
        );
    }
}

std::unique_ptr<dispatch_t>
dispatch_t::create(int argc,
                   char *argv[])
{
    using namespace boost::program_options;

    variables_map vm;

    options_description options;
    options.add_options()
        ("app", value<std::string>())
        ("uuid", value<std::string>())
        ("endpoint", value<std::string>())
        ("locator", value<std::string>());

    command_line_parser parser(argc, argv);
    parser.options(options);
    parser.allow_unregistered();
    store(parser.run(), vm);
    notify(vm);

    if (vm.count("app") == 0 ||
        vm.count("uuid") == 0 ||
        vm.count("endpoint") == 0 ||
        vm.count("locator") == 0)
    {
        std::cerr << "This is an application for Cocaine Engine. You can not run it like an ordinary application. Upload it in Cocaine." << std::endl;
        throw std::runtime_error("Invalid command line options");
    }

    service_manager_t::endpoint_t locator_endpoint("127.0.0.1", 10053);

    std::string raw_endpoint = vm["locator"].as<std::string>();
    size_t delim = raw_endpoint.rfind(':');

    if (delim == std::string::npos) {
        locator_endpoint.second = boost::lexical_cast<uint16_t>(raw_endpoint);
    } else {
        locator_endpoint.first = raw_endpoint.substr(0, delim);
        locator_endpoint.second = boost::lexical_cast<uint16_t>(raw_endpoint.substr(delim + 1));
    }

    // Block the deprecated signals.
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGPIPE);
    ::sigprocmask(SIG_BLOCK, &signals, nullptr);

    return std::unique_ptr<dispatch_t>(new dispatch_t(vm["app"].as<std::string>(),
                                                      vm["uuid"].as<std::string>(),
                                                      vm["endpoint"].as<std::string>(),
                                                      locator_endpoint));
}
