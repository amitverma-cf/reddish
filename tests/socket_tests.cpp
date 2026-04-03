#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <catch_amalgamated.hpp>

#include "error.hpp"
#include "socket.hpp"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

using namespace reddish;

namespace
{

void close_native_socket(
#ifdef _WIN32
    SOCKET socket
#else
    int socket
#endif
)
{
#ifdef _WIN32
    closesocket(socket);
#else
    ::close(socket);
#endif
}

} // namespace

TEST_CASE("socket accepts polls reads writes moves and closes")
{
    SocketSystem socket_system;
    Socket listener;
    std::uint16_t port = 27000;
    for (;; ++port)
    {
        try
        {
            listener.create_tcp();
            listener.bind(port);
            break;
        }
        catch (const Error &)
        {
            listener.close();
            REQUIRE(port < 28000);
        }
    }
    listener.listen(1);
    listener.set_nonblocking();

    std::atomic_bool client_connected = false;
    std::atomic_bool client_succeeded = false;
    std::string client_reply;
    std::thread client(
        [&]
        {
#ifdef _WIN32
            const SOCKET handle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (handle == INVALID_SOCKET) return;
#else
            const int handle = ::socket(AF_INET, SOCK_STREAM, 0);
            if (handle < 0) return;
#endif
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons(port);
            if (::connect(handle, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) !=
                0)
            {
                close_native_socket(handle);
                return;
            }
            client_connected = true;
            if (::send(handle, "ping", 4, 0) != 4)
            {
                close_native_socket(handle);
                return;
            }
            char reply[4];
            if (::recv(handle, reply, sizeof(reply), 0) != 4)
            {
                close_native_socket(handle);
                return;
            }
            client_reply.assign(reply, sizeof(reply));
            close_native_socket(handle);
            client_succeeded = true;
        });

    Socket accepted;
    for (int attempt = 0; attempt < 50 && !accepted.valid(); ++attempt)
    {
        accepted = listener.accept();
        if (!accepted.valid()) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(client_connected);
    REQUIRE(accepted.valid());
    accepted.set_nonblocking();

    char buffer[4];
    int received = accepted.receive(buffer, sizeof(buffer));
    if (received < 0)
    {
        CHECK(accepted.would_block());
        const auto events = socket_system.wait_for_events({{&accepted, false}}, 1000);
        REQUIRE(events.size() == 1);
        CHECK(events[0].readable);
        received = accepted.receive(buffer, sizeof(buffer));
    }
    REQUIRE(received == 4);
    CHECK(std::string(buffer, sizeof(buffer)) == "ping");
    REQUIRE(accepted.send("pong", 4) == 4);

    client.join();
    CHECK(client_succeeded);
    CHECK(client_reply == "pong");
    Socket moved = std::move(accepted);
    CHECK(moved.valid());
    CHECK_FALSE(accepted.valid());
    moved.close();
    CHECK_FALSE(moved.valid());
}
