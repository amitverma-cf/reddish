#include "socket.hpp"

#include <cstring>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace reddish
{

namespace
{
#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket invalid_socket = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr NativeSocket invalid_socket = -1;
#endif

NativeSocket native_handle(std::intptr_t handle)
{
    return static_cast<NativeSocket>(handle);
}

std::intptr_t portable_handle(NativeSocket handle)
{
    return static_cast<std::intptr_t>(handle);
}
} // namespace

SocketSystem::SocketSystem()
{
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed");
#endif
}

SocketSystem::~SocketSystem()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

std::vector<Socket *> SocketSystem::wait_for_readable(const std::vector<Socket *> &sockets) const
{
    if (sockets.empty()) return {};

#ifdef _WIN32
    std::vector<WSAPOLLFD> poll_fds;
    poll_fds.reserve(sockets.size());
    for (Socket *socket : sockets)
        poll_fds.push_back({native_handle(socket->handle_), POLLRDNORM, 0});

    if (WSAPoll(poll_fds.data(), static_cast<ULONG>(poll_fds.size()), -1) == SOCKET_ERROR)
        throw std::runtime_error("WSAPoll failed");
#else
    std::vector<pollfd> poll_fds;
    poll_fds.reserve(sockets.size());
    for (Socket *socket : sockets)
        poll_fds.push_back({native_handle(socket->handle_), POLLIN, 0});

    if (poll(poll_fds.data(), poll_fds.size(), -1) < 0) throw std::runtime_error("poll failed");
#endif

    std::vector<Socket *> ready_sockets;
    for (std::size_t index = 0; index < sockets.size(); ++index)
    {
#ifdef _WIN32
        const short ready_events = POLLRDNORM | POLLERR | POLLHUP | POLLNVAL;
#else
        const short ready_events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
#endif
        if (poll_fds[index].revents & ready_events) ready_sockets.push_back(sockets[index]);
    }

    return ready_sockets;
}

Socket::Socket() : handle_(portable_handle(invalid_socket)) {}

Socket::~Socket()
{
    close();
}

Socket::Socket(Socket &&other) noexcept : handle_(other.handle_)
{
    other.handle_ = portable_handle(invalid_socket);
}

Socket &Socket::operator=(Socket &&other) noexcept
{
    if (this == &other) return *this;
    close();
    handle_ = other.handle_;
    other.handle_ = portable_handle(invalid_socket);
    return *this;
}

void Socket::create_tcp()
{
#ifdef _WIN32
    const auto handle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    const auto handle = ::socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (handle == invalid_socket) throw std::runtime_error("Failed to create TCP socket");
    handle_ = portable_handle(handle);
}

void Socket::bind(std::uint16_t port)
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);

    int option = 1;
#ifdef _WIN32
    setsockopt(native_handle(handle_), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&option), sizeof(option));
#else
    setsockopt(native_handle(handle_), SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
#endif

    if (::bind(native_handle(handle_), reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
        throw std::runtime_error("Failed to bind TCP socket");
}

void Socket::listen(int backlog)
{
    if (::listen(native_handle(handle_), backlog) < 0)
        throw std::runtime_error("Failed to listen on TCP socket");
}

Socket Socket::accept()
{
    sockaddr_in address{};
#ifdef _WIN32
    int address_length = sizeof(address);
#else
    socklen_t address_length = sizeof(address);
#endif
    const auto client =
        ::accept(native_handle(handle_), reinterpret_cast<sockaddr *>(&address), &address_length);
    if (client == invalid_socket)
    {
        if (would_block()) return Socket{};
        throw std::runtime_error("Failed to accept TCP client");
    }

    Socket result;
    result.handle_ = portable_handle(client);
    return result;
}

void Socket::set_nonblocking()
{
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(native_handle(handle_), FIONBIO, &mode) != 0)
        throw std::runtime_error("Failed to set socket as nonblocking");
#else
    const int flags = fcntl(native_handle(handle_), F_GETFL, 0);
    if (flags < 0 || fcntl(native_handle(handle_), F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("Failed to set socket as nonblocking");
#endif
}

int Socket::receive(char *buffer, std::size_t length)
{
    return ::recv(native_handle(handle_), buffer, static_cast<int>(length), 0);
}

int Socket::send(const char *data, std::size_t length)
{
    return ::send(native_handle(handle_), data, static_cast<int>(length), 0);
}

bool Socket::would_block() const
{
#ifdef _WIN32
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

bool Socket::valid() const
{
    return native_handle(handle_) != invalid_socket;
}

void Socket::close()
{
    if (!valid()) return;
#ifdef _WIN32
    closesocket(native_handle(handle_));
#else
    ::close(native_handle(handle_));
#endif
    handle_ = portable_handle(invalid_socket);
}

} // namespace reddish
