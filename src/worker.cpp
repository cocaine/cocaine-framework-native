#include <cocaine/framework/worker.hpp>
#include <cocaine/framework/services/logger.hpp>

#include <cocaine/messages.hpp>

#include <stdexcept>
#include <boost/program_options.hpp>

using namespace cocaine;
using namespace cocaine::framework;

namespace {
class upstream_t:
    public api::stream_t
{
    enum class state_t: int {
        open,
        closed
    };
public:
    upstream_t(uint64_t id,
               worker_t * const worker):
        m_id(id),
        m_worker(worker),
        m_state(state_t::open)
    {
        // pass
    }

    virtual
    ~upstream_t() {
        if(m_state != state_t::closed) {
            close();
        }
    }

    virtual
    void
    write(const char * chunk,
         size_t size)
    {
        if(m_state == state_t::closed) {
            throw std::runtime_error("The stream has been closed.");
        } else {
            send<io::rpc::chunk>(std::string(chunk, size));
        }
    }

    virtual
    void
    error(error_code code,
          const std::string& message)
    {
        if(m_state == state_t::closed) {
            throw std::runtime_error("The stream has been closed.");
        } else {
            m_state = state_t::closed;
            send<io::rpc::error>(static_cast<int>(code), message);
            send<io::rpc::choke>();
        }
    }

    virtual
    void
    close() {
        if(m_state == state_t::closed) {
            throw std::runtime_error("The stream has been closed.");
        } else {
            m_state = state_t::closed;
            send<io::rpc::choke>();
        }
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
};

struct ignore_t {
    void
    operator()(const std::error_code& /* ec */) {
        // Empty.
    }
};

} // namespace

worker_t::worker_t(const std::string& name,
                   const std::string& uuid,
                   const std::string& endpoint):
    m_id(uuid),
    m_heartbeat_timer(m_service.native()),
    m_disown_timer(m_service.native()),
    m_service_manager(new service_manager_t(m_service)),
    m_app_name(name)
{
    m_log.reset(new log_t(
        m_service_manager->get_service<logging_service>(
            io::tcp::endpoint("127.0.0.1", 12501)
        ),
        cocaine::format("worker/%s", name)
    ));

    auto socket = std::make_shared<io::socket<io::local>>(io::local::endpoint(endpoint));

    m_channel.reset(new io::channel<io::socket<io::local>>(m_service, socket));

    m_channel->rd->bind(std::bind(&worker_t::on_message, this, std::placeholders::_1), ignore_t());
    m_channel->wr->bind(ignore_t());

    // Greet the engine!
    send<io::rpc::handshake>(0ul, m_id);

    m_heartbeat_timer.set<worker_t, &worker_t::on_heartbeat>(this);
    m_heartbeat_timer.start(0.0f, 5.0f);

    m_disown_timer.set<worker_t, &worker_t::on_disown>(this);
    m_disown_timer.start(2.0f);
}

worker_t::~worker_t() {
    // pass
}

void
worker_t::run() {
    if (m_application) {
        m_service.run();
    }
}

void
worker_t::on_message(const io::message_t& message) {
    COCAINE_LOG_DEBUG(
        m_log,
        "worker %s received type %d message",
        m_id,
        message.id()
    );

    switch(message.id()) {
        case io::event_traits<io::rpc::heartbeat>::id: {
            m_disown_timer.stop();
            break;
        }

        case io::event_traits<io::rpc::invoke>::id: {
            std::string event;
            message.as<io::rpc::invoke>(event);

            COCAINE_LOG_DEBUG(m_log, "worker %s invoking session %s with event '%s'", m_id, message.band(), event);

            std::shared_ptr<api::stream_t> upstream(
                std::make_shared<upstream_t>(message.band(), this)
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
                    it->second.downstream->write(chunk.data(), chunk.size());
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
                    it->second.downstream->close();
                } catch(const std::exception& e) {
                    it->second.upstream->error(invocation_error, e.what());
                } catch(...) {
                    it->second.upstream->error(invocation_error, "unexpected exception");
                }

                m_streams.erase(it);
            }

            break;
        }

        case io::event_traits<io::rpc::terminate>::id: {
            terminate(io::rpc::terminate::normal, "per request");
            break;
        }

        default: {
            COCAINE_LOG_WARNING(
                m_log,
                "worker %s dropping unknown type %d message",
                m_id,
                message.id()
            );
        }
    }
}

void
worker_t::on_heartbeat(ev::timer&,
                       int)
{
    send<io::rpc::heartbeat>(0ul);
    m_disown_timer.start(2.0f);
}

void
worker_t::on_disown(ev::timer&,
                    int)
{
    COCAINE_LOG_ERROR(
        m_log,
        "worker %s has lost the controlling engine",
        m_id
    );

    m_service.native().unloop(ev::ALL);
}

void
worker_t::terminate(int reason,
                    const std::string& message)
{
    send<io::rpc::terminate>(0ul, reason, message);
    m_service.native().unloop(ev::ALL);
}


std::shared_ptr<worker_t>
cocaine::framework::make_worker(int argc,
                              char *argv[])
{
    using namespace boost::program_options;

    variables_map vm;

    options_description options;
    options.add_options()
        ("app", value<std::string>())
        ("uuid", value<std::string>())
        ("endpoint", value<std::string>());

    try {
        command_line_parser parser(argc, argv);
        parser.options(options);
        parser.allow_unregistered();
        store(parser.run(), vm);
        notify(vm);
    } catch(const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        throw;
    }

    try {
        return std::make_shared<worker_t>(vm["app"].as<std::string>(),
                                          vm["uuid"].as<std::string>(),
                                          vm["endpoint"].as<std::string>());
    } catch(const std::exception& e) {
        std::cerr << cocaine::format("ERROR: unable to start the worker - %s", e.what()) << std::endl;
        throw;
    }
}
