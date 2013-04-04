#ifndef COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP
#define COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP

#include <cocaine/framework/service.hpp>
#include <cocaine/framework/logging.hpp>

namespace cocaine { namespace framework {

class logging_service_t :
    public service_t,
    public logger_t
{
public:
    logging_service_t(const std::string& name,
                      cocaine::io::reactor_t& service,
                      const cocaine::io::tcp::endpoint& resolver) :
        service_t(name, service, resolver),
        m_priority(cocaine::logging::priorities::warning)
    {
        // pass
    }

    void
    emit(cocaine::logging::priorities priority,
         const std::string& source,
         const std::string& message)
    {
        call<cocaine::io::logging::emit>(ignore_message,
                                         priority,
                                         source,
                                         message);
    }

    cocaine::logging::priorities
    verbosity() const {
        return m_priority;
    }

protected:
    void initialize() {
        call<cocaine::io::logging::verbosity>(
            std::bind(&logging_service_t::on_verbosity_response,
                      shared_from_this(),
                      std::placeholders::_1)
        );
    }

    void
    on_verbosity_response(const cocaine::io::message_t& message) {
        if (message.id() == io::event_traits<io::rpc::chunk>::id) {
            std::string chunk;
            message.as<io::rpc::chunk>(chunk);
            msgpack::unpacked msg;
            msgpack::unpack(&msg, chunk.data(), chunk.size());
            cocaine::io::type_traits<cocaine::logging::priorities>::unpack(msg.get(), m_priority);
        }
    }

private:
    cocaine::logging::priorities m_priority;
};

}} // namespace cocaine::framework

#endif // COCAINE_FRAMEWORK_SERVICES_LOGGER_HPP
