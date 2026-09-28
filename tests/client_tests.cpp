#include "client.hpp"
#include "server.hpp"
#include <catch_amalgamated.hpp>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <thread>

namespace
{
std::expected<std::string, std::string> reply_from_peer(const std::vector<std::string> &fragments)
{
    reddish::SocketSystem sockets;
    reddish::Socket listener;
    listener.create_tcp();
    listener.bind(26992);
    listener.listen(1);
    listener.set_nonblocking();
    sockets.watch(listener, 1, true, false);
    std::exception_ptr failure;
    std::jthread worker(
        [&]
        {
            try
            {
                if (sockets.wait(2000).empty()) throw std::runtime_error("peer accept timeout");
                auto peer = listener.accept();
                sockets.unwatch(listener);
                peer.set_nonblocking();
                peer.set_nodelay();
                sockets.watch(peer, 2, true, false);
                if (sockets.wait(2000).empty()) throw std::runtime_error("peer request timeout");
                char request[1024];
                if (peer.receive(request, sizeof(request)) <= 0)
                    throw std::runtime_error("peer did not receive command");
                for (const auto &fragment : fragments)
                {
                    std::size_t sent = 0;
                    while (sent < fragment.size())
                    {
                        const auto count =
                            peer.send(fragment.data() + sent, fragment.size() - sent);
                        if (count > 0) sent += static_cast<std::size_t>(count);
                        else if (count < 0 && peer.would_block())
                        {
                            sockets.watch(peer, 2, false, true);
                            if (sockets.wait(2000).empty())
                                throw std::runtime_error("peer send timeout");
                        }
                        else throw std::runtime_error("peer send failed");
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
                sockets.unwatch(peer);
            }
            catch (...)
            {
                failure = std::current_exception();
            }
        });
    reddish::client::Client client("127.0.0.1", 26992);
    const auto connected = client.connect();
    auto result = connected
                      ? client.ping()
                      : std::expected<std::string, std::string>(std::unexpected(connected.error()));
    worker.join();
    if (failure) std::rethrow_exception(failure);
    return result;
}
} // namespace

TEST_CASE("C++ client assembles fragmented nested RESP replies")
{
    const std::vector<std::string> fragments = {"*2\r",      "\n$5\r\nh",    "ello\r",
                                                "\n*2\r\n:", "-42\r\n$-1\r", "\n"};
    REQUIRE(reply_from_peer(fragments) == "*2\r\n$5\r\nhello\r\n*2\r\n:-42\r\n$-1\r\n");
}

TEST_CASE("C++ client rejects malformed and truncated RESP replies")
{
    const auto reply = GENERATE(
        std::string(":abc\r\n"), std::string(":\r\n"), std::string(":9223372036854775808\r\n"),
        std::string("$-2\r\n"), std::string("$3\r\nabcXX"), std::string("$16777216\r\n"),
        std::string("?bad\r\n"), std::string("$5\r\nabc"), std::string("*2\r\n+only-one\r\n"));
    INFO(reply);
    CHECK_FALSE(reply_from_peer({reply}));
}

TEST_CASE("C++ client reads large binary replies and keeps command boundaries")
{
    reddish::ServerConfig config;
    config.port = 26991;
    reddish::Server server(config);
    std::jthread worker([&] { server.start(); });
    struct Stop
    {
        reddish::Server &server;
        ~Stop()
        {
            server.stop();
        }
    } stop{server};
    reddish::client::Client client("127.0.0.1", config.port);
    bool connected = false;
    for (int attempt = 0; attempt < 100 && !connected; ++attempt)
    {
        connected = client.connect().has_value();
        if (!connected) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(connected);
    std::string value(128 * 1024, 'x');
    value[123] = '\0';
    value.replace(4094, 4, "\r\n\r\n");
    REQUIRE(client.set("large", value) == "+OK\r\n");
    REQUIRE(client.get("large") == "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n");
    REQUIRE(client.ping() == "+PONG\r\n");
    REQUIRE(client.get("missing") == "$-1\r\n");
    REQUIRE(client.connect());
    REQUIRE(client.ping() == "+PONG\r\n");
}
