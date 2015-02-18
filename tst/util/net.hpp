#pragma once

#include <cstdint>

#include <boost/thread/barrier.hpp>
#include <boost/thread/thread.hpp>

#include <asio/ip/tcp.hpp>

#include <cocaine/framework/forwards.hpp>

/// Alias for asyncronous i/o implementation namespace (either boost::asio or pure asio).
namespace testing {

namespace util {

namespace fw = cocaine::framework;

static const std::uint64_t TIMEOUT = 1000;

/// An OS should select available port for us.
std::uint16_t port();

class server_t {
    fw::loop_t loop;
    std::unique_ptr<fw::loop_t::work> work;
    boost::thread thread;

public:
    std::vector<std::shared_ptr<asio::ip::tcp::socket>> sockets;

    server_t(std::uint16_t port, std::function<void(asio::ip::tcp::acceptor&, fw::loop_t&)> fn) :
        work(new fw::loop_t::work(loop))
    {
        boost::barrier barrier(2);
        thread = std::move(boost::thread([this, port, fn, &barrier]{
            asio::ip::tcp::acceptor acceptor(loop);
            asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
            acceptor.open(endpoint.protocol());
            acceptor.bind(endpoint);
            acceptor.listen();

            barrier.wait();

            fn(acceptor, loop);
        }));

        barrier.wait();
    }

    ~server_t() {
        work.reset();
        thread.join();
    }

    void stop() {
        work.reset();
    }
};

class client_t {
    fw::loop_t io;
    std::unique_ptr<fw::loop_t::work> work;
    boost::thread thread;

public:
    client_t() :
        work(new fw::loop_t::work(io)),
        thread(std::bind(static_cast<std::size_t(fw::loop_t::*)()>(&fw::loop_t::run), std::ref(io)))
    {}

    ~client_t() {
        work.reset();
        thread.join();
    }

    fw::loop_t& loop() {
        return io;
    }
};

} // namespace util

} // namespace testing
