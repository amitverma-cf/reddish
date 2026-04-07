#include <catch_amalgamated.hpp>

#include "server.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace reddish;

namespace
{

std::atomic_bool stop_server = false;

bool should_stop_server()
{
    return stop_server.load();
}

class ClientSocket
{
  public:
    ClientSocket()
    {
#ifdef _WIN32
        handle_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (handle_ == INVALID_SOCKET) throw std::runtime_error("could not create test socket");
#else
        handle_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (handle_ < 0) throw std::runtime_error("could not create test socket");
#endif
    }

    ~ClientSocket()
    {
#ifdef _WIN32
        if (handle_ != INVALID_SOCKET) closesocket(handle_);
#else
        if (handle_ >= 0) ::close(handle_);
#endif
    }

    bool connect(std::uint16_t port)
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        return ::connect(handle_, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) ==
               0;
    }

    void send_all(std::string_view input)
    {
        std::size_t sent = 0;
        while (sent < input.size())
        {
            const auto result =
                ::send(handle_, input.data() + sent, static_cast<int>(input.size() - sent), 0);
            if (result <= 0) throw std::runtime_error("could not send test request");
            sent += static_cast<std::size_t>(result);
        }
    }

    std::string receive_until(std::string_view expected_suffix)
    {
        std::string response;
        char buffer[256];
        while (!response.ends_with(expected_suffix))
        {
            const auto received = ::recv(handle_, buffer, static_cast<int>(sizeof(buffer)), 0);
            if (received <= 0) throw std::runtime_error("could not receive test response");
            response.append(buffer, static_cast<std::size_t>(received));
        }
        return response;
    }

    bool receives_disconnect()
    {
        char byte = 0;
        return ::recv(handle_, &byte, 1, 0) <= 0;
    }

  private:
#ifdef _WIN32
    SOCKET handle_;
#else
    int handle_;
#endif
};

std::uint16_t find_open_port()
{
    for (std::uint16_t port = 24000; port < 25000; ++port)
    {
        ClientSocket client;
        if (!client.connect(port)) return port;
    }
    throw std::runtime_error("no test port available");
}

} // namespace

TEST_CASE("server accepts TCP commands and cleanly stops")
{
    SocketSystem socket_system;
    const auto port = find_open_port();
    const auto dump_path = std::filesystem::temp_directory_path() / "reddish-server-test.reddish";
    std::error_code error;
    std::filesystem::remove(dump_path, error);

    stop_server = false;
    Server server({port, 32, std::chrono::hours(1), dump_path});
    std::exception_ptr server_error;
    std::thread thread(
        [&]
        {
            try
            {
                server.start(should_stop_server);
            }
            catch (...)
            {
                server_error = std::current_exception();
            }
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ClientSocket client;
    REQUIRE(client.connect(port));

    client.send_all("*1\r\n$4\r\nPING\r\n*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$7\r\nreddish\r\n"
                    "*2\r\n$3\r\nGET\r\n$4\r\nname\r\n");
    CHECK(client.receive_until("$7\r\nreddish\r\n") == "+PONG\r\n+OK\r\n$7\r\nreddish\r\n");

    stop_server = true;
    thread.join();
    CHECK_FALSE(server_error);
    CHECK(std::filesystem::exists(dump_path));
    std::filesystem::remove(dump_path, error);
}

TEST_CASE("server reports and disconnects an oversized input buffer")
{
    SocketSystem socket_system;
    const auto port = find_open_port();
    const auto dump_path =
        std::filesystem::temp_directory_path() / "reddish-backpressure-test.reddish";
    std::error_code error;
    std::filesystem::remove(dump_path, error);

    stop_server = false;
    Server server({port, 32, std::chrono::hours(1), dump_path});
    std::exception_ptr server_error;
    std::thread thread(
        [&]
        {
            try
            {
                server.start(should_stop_server);
            }
            catch (...)
            {
                server_error = std::current_exception();
            }
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ClientSocket client;
    REQUIRE(client.connect(port));
    client.send_all("*1\r\n$2097152\r\n" + std::string(1024 * 1024 + 1, 'x'));
    CHECK(client.receive_until("\r\n") == "-ERR request exceeds maximum buffer size\r\n");

    stop_server = true;
    thread.join();
    CHECK_FALSE(server_error);
    std::filesystem::remove(dump_path, error);
}

TEST_CASE("server reports malformed RESP before closing the connection")
{
    SocketSystem socket_system;
    const auto port = find_open_port();
    const auto dump_path = std::filesystem::temp_directory_path() / "reddish-protocol-test.reddish";
    std::error_code error;
    std::filesystem::remove(dump_path, error);

    stop_server = false;
    Server server({port, 32, std::chrono::hours(1), dump_path});
    std::exception_ptr server_error;
    std::thread thread(
        [&]
        {
            try
            {
                server.start(should_stop_server);
            }
            catch (...)
            {
                server_error = std::current_exception();
            }
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ClientSocket client;
    REQUIRE(client.connect(port));
    client.send_all("+not-resp\r\n");
    CHECK(client.receive_until("\r\n") ==
          "-ERR Protocol error: Expected RESP array prefix '*'\r\n");

    stop_server = true;
    thread.join();
    CHECK_FALSE(server_error);
    std::filesystem::remove(dump_path, error);
}

TEST_CASE("server disconnects clients when queued responses exceed the output limit")
{
    SocketSystem socket_system;
    const auto port = find_open_port();
    const auto dump_path = std::filesystem::temp_directory_path() / "reddish-output-test.reddish";
    std::error_code error;
    std::filesystem::remove(dump_path, error);

    stop_server = false;
    Server server({port, 32, std::chrono::hours(1), dump_path});
    std::exception_ptr server_error;
    std::thread thread(
        [&]
        {
            try
            {
                server.start(should_stop_server);
            }
            catch (...)
            {
                server_error = std::current_exception();
            }
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ClientSocket client;
    REQUIRE(client.connect(port));
    const std::string value(600 * 1024, 'v');
    client.send_all("*3\r\n$3\r\nSET\r\n$3\r\nbig\r\n$614400\r\n" + value + "\r\n");
    REQUIRE(client.receive_until("\r\n") == "+OK\r\n");

    client.send_all("*2\r\n$3\r\nGET\r\n$3\r\nbig\r\n*2\r\n$3\r\nGET\r\n$3\r\nbig\r\n");
    CHECK(client.receives_disconnect());

    stop_server = true;
    thread.join();
    CHECK_FALSE(server_error);
    std::filesystem::remove(dump_path, error);
}
