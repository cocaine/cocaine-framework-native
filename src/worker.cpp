#include <cocaine/framework/worker.hpp>

#include <cocaine/messages.hpp>

#include <csignal>
#include <stdexcept>
#include <iostream>
#include <boost/program_options.hpp>

using namespace cocaine;
using namespace cocaine::framework;

namespace {
class worker_upstream_t:
    public upstream_t
{
    enum class state_t: int {
        open,
        closed
    };
public:
    worker_upstream_t(uint64_t id,
                      worker_t * const worker):
        m_id(id),
        m_worker(worker),
        m_state(state_t::open)
    {
        // pass
    }

    ~worker_upstream_t() {
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
            throw std::runtime_error("The stream has been closed.");
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
            throw std::runtime_error("The stream has been closed.");
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
        m_worker->send<Event>(m_id, std::forward<Args>(args)...);
    }

private:
    const uint64_t m_id;
    worker_t * const m_worker;
    state_t m_state;

    std::mutex m_closed_lock;
};

class killer_t {
public:
    killer_t()
    {
        // pass
    }

    void
    operator()(const std::error_code& code) {
        throw socket_error_t(cocaine::format("socket error with code %d in worker", code));
    }
};

struct ioservice_executor_t {
    ioservice_executor_t(cocaine::io::reactor_t& ioservice) :
        m_ioservice(ioservice)
    {
        // pass
    }

    ioservice_executor_t(const ioservice_executor_t& other) :
        m_ioservice(other.m_ioservice)
    {
        // pass
    }

    void
    operator()(const std::function<void()>& f) {
        m_ioservice.post(f);
    }

private:
    cocaine::io::reactor_t& m_ioservice;
};

} // namespace

worker_t::worker_t(const std::string& name,
                   const std::string& uuid,
                   const std::string& endpoint,
                   uint16_t resolver_port):
    m_id(uuid),
    m_heartbeat_timer(m_ioservice.native()),
    m_disown_timer(m_ioservice.native()),
    m_app_name(name)
{
    auto socket = std::make_shared<io::socket<io::local>>(io::local::endpoint(endpoint));
    m_channel.reset(new io::channel<io::socket<io::local>>(m_ioservice, socket));
    m_channel->rd->bind(std::bind(&worker_t::on_message, this, std::placeholders::_1),
                        killer_t());
    m_channel->wr->bind(killer_t());

    // Greet the engine!
    send<io::rpc::handshake>(0ul, m_id);

    m_heartbeat_timer.set<worker_t, &worker_t::on_heartbeat>(this);
    m_heartbeat_timer.start(0.0f, 5.0f);

    m_disown_timer.set<worker_t, &worker_t::on_disown>(this);

    // Set the lowest priority for the disown timer.
    m_disown_timer.priority = EV_MINPRI;

    m_service_manager.reset(
        new service_manager_t(cocaine::io::tcp::endpoint("127.0.0.1", resolver_port),
                              cocaine::format("app/%s", name),
                              ioservice_executor_t(m_ioservice))
    );
    m_log = m_service_manager->get_system_logger();
}

worker_t::~worker_t() {
    // pass
}

void
worker_t::on_message(const io::message_t& message) {
    COCAINE_LOG_DEBUG(m_log, "worker %s received type %d message", m_id, message.id());

    switch(message.id()) {
        case io::event_traits<io::rpc::heartbeat>::id: {
            m_disown_timer.stop();
            break;
        }
        case io::event_traits<io::rpc::invoke>::id: {
            std::string event;
            message.as<io::rpc::invoke>(event);

            COCAINE_LOG_DEBUG(m_log,
                              "worker %s invoking session %s with event '%s'",
                              m_id,
                              message.band(),
                              event);

            std::shared_ptr<worker_upstream_t> upstream(
                std::make_shared<worker_upstream_t>(message.band(), this)
            );

            try {
                io_pair_t io = {
                    upstream,
                    m_application->invoke(event, upstream)
                };

                m_streams.insert(std::make_pair(message.band(), io));
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

            stream_map_t::iterator it(m_streams.find(message.band()));

            // NOTE: This may be a chunk for a failed invocation, in which case there
            // will be no active stream, so drop the message.
            if(it != m_streams.end()) {
                try {
                    it->second.handler->on_chunk(chunk.data(), chunk.size());
                } catch(const std::exception& e) {
                    it->second.upstream->error(invocation_error, e.what());
                    m_streams.erase(it);
                } catch(...) {
                    it->second.upstream->error(invocation_error, "unexpected exception");
                    m_streams.erase(it);
                }
            }

            break;
        }
        case io::event_traits<io::rpc::choke>::id: {
            stream_map_t::iterator it = m_streams.find(message.band());

            // NOTE: This may be a choke for a failed invocation, in which case there
            // will be no active stream, so drop the message.
            if(it != m_streams.end()) {
                try {
                    it->second.handler->on_close();
                } catch(const std::exception& e) {
                    it->second.upstream->error(invocation_error, e.what());
                } catch(...) {
                    it->second.upstream->error(invocation_error, "unexpected exception");
                }

                m_streams.erase(it);
            }

            break;
        }
        case io::event_traits<io::rpc::error>::id: {
            int ec;
            std::string error_message;
            message.as<io::rpc::error>(ec, error_message);

            stream_map_t::iterator it(m_streams.find(message.band()));

            // NOTE: This may be a chunk for a failed invocation, in which case there
            // will be no active stream, so drop the message.
            if(it != m_streams.end()) {
                try {
                    it->second.handler->on_error(ec, error_message);
                } catch(const std::exception& e) {
                    it->second.upstream->error(invocation_error, e.what());
                    m_streams.erase(it);
                } catch(...) {
                    it->second.upstream->error(invocation_error, "unexpected exception");
                    m_streams.erase(it);
                }
            }

            break;
        }
        case io::event_traits<io::rpc::terminate>::id: {
            terminate(io::rpc::terminate::normal, "per request");
            break;
        }
        default: {
            COCAINE_LOG_WARNING(m_log,
                                "worker %s dropping unknown type %d message",
                                m_id,
                                message.id());
        }
    }
}

void
worker_t::on_heartbeat(ev::timer&,
                       int)
{
    send<io::rpc::heartbeat>(0ul);
    m_disown_timer.start(60.0f);
}

void
worker_t::on_disown(ev::timer&,
                    int)
{
    COCAINE_LOG_ERROR(m_log, "worker %s has lost the controlling engine", m_id);
    m_ioservice.native().unloop(ev::ALL);
}

void
worker_t::terminate(int code,
                    const std::string& reason)
{
    send<io::rpc::terminate>(0ul, static_cast<io::rpc::terminate::code>(code), reason);
    m_ioservice.native().unloop(ev::ALL);
}

int
worker_t::run() {
    if (m_application) {
        m_ioservice.run();
        return 0;
    }
    else {
        terminate(cocaine::io::rpc::terminate::abnormal,
                  "Application object has not been created.");
        return 1;
    }
}

std::shared_ptr<worker_t>
worker_t::create(int argc,
                 char *argv[])
{
    using namespace boost::program_options;

    variables_map vm;

    options_description options;
    options.add_options()
        ("app", value<std::string>())
        ("uuid", value<std::string>())
        ("endpoint", value<std::string>())
        ("locator", value<uint16_t>());

    try {
        command_line_parser parser(argc, argv);
        parser.options(options);
        parser.allow_unregistered();
        store(parser.run(), vm);
        notify(vm);
    } catch(const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        throw 1;
    }

    if (vm.count("app") == 0 ||
        vm.count("uuid") == 0 ||
        vm.count("endpoint") == 0 ||
        vm.count("locator") == 0)
    {
        std::cerr << "This is an application for Cocaine Engine. You can not run it like an ordinary application. Upload it in Cocaine." << std::endl;
        throw 1;
    }

    // Block the deprecated signals.

    sigset_t signals;

    sigemptyset(&signals);
    sigaddset(&signals, SIGPIPE);

    ::sigprocmask(SIG_BLOCK, &signals, nullptr);

    try {
        return std::shared_ptr<worker_t>(new worker_t(vm["app"].as<std::string>(),
                                                      vm["uuid"].as<std::string>(),
                                                      vm["endpoint"].as<std::string>(),
                                                      vm["locator"].as<uint16_t>()));
    } catch(const std::exception& e) {
        std::cerr << cocaine::format("ERROR: unable to start the worker - %s", e.what()) << std::endl;
        throw 1;
    }
}
