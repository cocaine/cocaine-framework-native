#include <cocaine/framework/service.hpp>

using namespace cocaine::framework;

void
logging_service_t::initialize() {
    cocaine::io::reactor_t ioservice;

    auto socket = std::make_shared<cocaine::io::socket<cocaine::io::tcp>>(
        cocaine::io::tcp::endpoint(endpoint().first, endpoint().second)
    );

    cocaine::io::channel<
        cocaine::io::socket<cocaine::io::tcp>
    > channel(ioservice, socket);

    channel.rd->bind(std::bind(&logging_service_t::on_verbosity_response,
                               shared_from_this(),
                               &ioservice,
                               std::placeholders::_1),
                     std::bind(&logging_service_t::on_error, this, std::placeholders::_1));

    channel.wr->write<cocaine::io::logging::verbosity>(0ul);

    ioservice.run();
}

void
logging_service_t::on_verbosity_response(cocaine::io::reactor_t *ioservice,
                                         const cocaine::io::message_t& message)
{
    if (message.id() == io::event_traits<io::rpc::chunk>::id) {
        std::string chunk;
        message.as<io::rpc::chunk>(chunk);
        msgpack::unpacked msg;
        msgpack::unpack(&msg, chunk.data(), chunk.size());
        cocaine::io::type_traits<cocaine::logging::priorities>::unpack(msg.get(), m_priority);
    } else if (message.id() == io::event_traits<io::rpc::choke>::id) {
        ioservice->native().unloop(ev::ALL);
    }
}
