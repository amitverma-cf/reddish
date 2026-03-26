#include "socket.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace reddish::client
{
SocketSystem::SocketSystem()
{
#ifdef _WIN32
    WSADATA data{};
    WSAStartup(MAKEWORD(2, 2), &data);
#endif
}
SocketSystem::~SocketSystem()
{
#ifdef _WIN32
    WSACleanup();
#endif
}
Socket::~Socket()
{
    close();
}
std::expected<void, std::string> Socket::connect(const std::string &host, std::uint16_t port)
{
    const auto socket = ::socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (socket == INVALID_SOCKET) return std::unexpected("Failed to create socket");
#else
    if (socket < 0) return std::unexpected("Failed to create socket");
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1 ||
        ::connect(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
    {
#ifdef _WIN32
        closesocket(socket);
#else
        ::close(socket);
#endif
        return std::unexpected("Failed to connect to server");
    }
    handle_ = static_cast<std::intptr_t>(socket);
    return {};
}
std::expected<int, std::string> Socket::send(const char *data, std::size_t size)
{
    const auto count =
        ::send(static_cast<decltype(::socket(0, 0, 0))>(handle_), data, static_cast<int>(size), 0);
    if (count < 0) return std::unexpected("Failed to send command");
    return count;
}
std::expected<int, std::string> Socket::receive(char *data, std::size_t size)
{
    const auto count =
        ::recv(static_cast<decltype(::socket(0, 0, 0))>(handle_), data, static_cast<int>(size), 0);
    if (count <= 0) return std::unexpected("Server closed connection");
    return count;
}
void Socket::close()
{
    if (handle_ == -1) return;
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(handle_));
#else
    ::close(static_cast<int>(handle_));
#endif
    handle_ = -1;
}
} // namespace reddish::client
