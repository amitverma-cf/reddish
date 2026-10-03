#include "socket.hpp"
#include "error.hpp"

#include <cstring>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/epoll.h>
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

constexpr std::size_t max_events_per_wait = 256;
}

struct SocketSystem::Platform
{
    struct Registration
    {
        Socket *socket;
        int token;
        bool writable;
    };

    std::vector<Registration> registrations;
    std::unordered_map<std::intptr_t, std::size_t> slots;
    std::vector<SocketEvent> ready;

#ifdef __linux__
    int epoll_fd = -1;
    std::vector<epoll_event> raw;
#endif
#ifdef _WIN32
    std::vector<WSAPOLLFD> poll_fds;
#endif
};

SocketSystem::SocketSystem() : platform_(std::make_unique<Platform>())
{
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw Error{ErrorCode::socket_startup_failed};
#elif defined(__linux__)
    platform_->epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
    if (platform_->epoll_fd < 0) throw Error{ErrorCode::socket_startup_failed};
    platform_->raw.resize(max_events_per_wait);
#endif
}

SocketSystem::~SocketSystem()
{
#ifdef _WIN32
    WSACleanup();
#elif defined(__linux__)
    if (platform_->epoll_fd >= 0) ::close(platform_->epoll_fd);
#endif
}

void SocketSystem::watch(Socket &socket, int token, bool readable, bool writable)
{
    const auto handle = socket.handle_;

#if defined(__linux__)
    epoll_event interest{};
    interest.events = (readable ? static_cast<std::uint32_t>(EPOLLIN) : 0U) |
                      (writable ? static_cast<std::uint32_t>(EPOLLOUT) : 0U);
    interest.data.u64 = static_cast<std::uint64_t>(static_cast<std::uint32_t>(token));

    if (::epoll_ctl(platform_->epoll_fd, EPOLL_CTL_ADD, native_handle(handle), &interest) == 0)
        return;
    if (errno != EEXIST) throw Error{ErrorCode::socket_poll_failed};
    if (::epoll_ctl(platform_->epoll_fd, EPOLL_CTL_MOD, native_handle(handle), &interest) != 0)
        throw Error{ErrorCode::socket_poll_failed};
#else
    const short events =
        static_cast<short>((readable ? POLLRDNORM : 0) | (writable ? POLLWRNORM : 0));

    const auto existing = platform_->slots.find(handle);
    if (existing != platform_->slots.end())
    {
        const std::size_t index = existing->second;
        platform_->registrations[index].writable = writable;
        platform_->poll_fds[index].events = events;
        return;
    }

    const std::size_t index = platform_->registrations.size();
    platform_->registrations.push_back({&socket, token, writable});
    platform_->slots.emplace(handle, index);

    WSAPOLLFD descriptor{};
    descriptor.fd = native_handle(handle);
    descriptor.events = events;
    platform_->poll_fds.push_back(descriptor);
#endif
}

void SocketSystem::unwatch(Socket &socket) noexcept
{
    const auto handle = socket.handle_;

#if defined(__linux__)
    ::epoll_ctl(platform_->epoll_fd, EPOLL_CTL_DEL, native_handle(handle), nullptr);
#else
    const auto existing = platform_->slots.find(handle);
    if (existing == platform_->slots.end()) return;

    const std::size_t index = existing->second;
    const std::size_t last = platform_->registrations.size() - 1;
    if (index != last)
    {
        platform_->registrations[index] = platform_->registrations[last];
        platform_->poll_fds[index] = platform_->poll_fds[last];
        platform_->slots[platform_->registrations[index].socket->handle_] = index;
    }
    platform_->registrations.pop_back();
    platform_->poll_fds.pop_back();
    platform_->slots.erase(existing);
#endif
}

std::span<const SocketEvent> SocketSystem::wait(int timeout_ms)
{
    platform_->ready.clear();

#if defined(__linux__)
    const int count = ::epoll_wait(platform_->epoll_fd, platform_->raw.data(),
                                   static_cast<int>(platform_->raw.size()), timeout_ms);

    if (count < 0)
    {
        if (errno == EINTR) return platform_->ready;
        throw Error{ErrorCode::socket_poll_failed};
    }

    for (int index = 0; index < count; ++index)
    {
        const auto &event = platform_->raw[static_cast<std::size_t>(index)];
        SocketEvent translated;
        translated.token = static_cast<int>(static_cast<std::uint32_t>(event.data.u64));
        translated.readable = (event.events & EPOLLIN) != 0;
        translated.writable = (event.events & EPOLLOUT) != 0;

        translated.error = (event.events & EPOLLERR) != 0;
        translated.readable = translated.readable || (event.events & EPOLLHUP) != 0;
        platform_->ready.push_back(translated);
    }
#else
    if (platform_->poll_fds.empty())
    {
        if (timeout_ms > 0) ::Sleep(static_cast<DWORD>(timeout_ms));
        return platform_->ready;
    }

    const int result = WSAPoll(platform_->poll_fds.data(),
                               static_cast<ULONG>(platform_->poll_fds.size()), timeout_ms);
    if (result == SOCKET_ERROR)
    {
        if (WSAGetLastError() == WSAEINTR) return platform_->ready;
        throw Error{ErrorCode::socket_poll_failed};
    }

    for (std::size_t index = 0; index < platform_->poll_fds.size(); ++index)
    {
        const auto &descriptor = platform_->poll_fds[index];
        SocketEvent translated;
        translated.token = platform_->registrations[index].token;
        translated.readable = (descriptor.revents & POLLRDNORM) != 0;
        translated.writable = (descriptor.revents & POLLWRNORM) != 0;
        translated.error = (descriptor.revents & POLLNVAL) != 0;
        translated.readable = translated.readable || (descriptor.revents & POLLERR) != 0;
        translated.readable = translated.readable || (descriptor.revents & POLLHUP) != 0;
        if (!translated.readable && !translated.writable && !translated.error) continue;
        platform_->ready.push_back(translated);
        if (platform_->ready.size() >= max_events_per_wait) break;
    }
#endif

    return platform_->ready;
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
    if (handle == invalid_socket) throw Error{ErrorCode::socket_creation_failed};
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
        throw Error{ErrorCode::socket_bind_failed};
}

void Socket::listen(int backlog)
{
    if (::listen(native_handle(handle_), backlog) < 0) throw Error{ErrorCode::socket_listen_failed};
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
        throw Error{ErrorCode::socket_accept_failed};
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
        throw Error{ErrorCode::socket_nonblocking_failed};
#else
    const int flags = fcntl(native_handle(handle_), F_GETFL, 0);
    if (flags < 0 || fcntl(native_handle(handle_), F_SETFL, flags | O_NONBLOCK) < 0)
        throw Error{ErrorCode::socket_nonblocking_failed};
#endif
}

void Socket::set_nodelay()
{
    const int option = 1;
#ifdef _WIN32
    setsockopt(native_handle(handle_), IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char *>(&option), sizeof(option));
#else
    setsockopt(native_handle(handle_), IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option));
#endif
}

int Socket::receive(char *buffer, std::size_t length)
{
    return ::recv(native_handle(handle_), buffer, static_cast<int>(length), 0);
}

int Socket::send(const char *data, std::size_t length)
{
#ifdef _WIN32
    return ::send(native_handle(handle_), data, static_cast<int>(length), 0);
#else
    return ::send(native_handle(handle_), data, length, MSG_NOSIGNAL);
#endif
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

}
