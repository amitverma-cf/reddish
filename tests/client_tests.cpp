#include "client.hpp"
#include "server.hpp"
#include <catch_amalgamated.hpp>
#include <chrono>
#include <thread>

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
