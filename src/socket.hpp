#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace reddish
{

class Socket;

struct SocketEvent
{
    int token = -1;
    bool readable = false;
    bool writable = false;
    bool error = false;
};

// Readiness notification.
//
// On Linux this is a real epoll instance: descriptors are registered once and the
// kernel keeps the interest set, so a wait costs one syscall and no per-call
// bookkeeping. On Windows, where there is no epoll, the same persistent
// registration is kept in a WSAPoll array that is only touched when the set
// changes.
class SocketSystem
{
  public:
    SocketSystem();
    ~SocketSystem();

    SocketSystem(const SocketSystem &) = delete;
    SocketSystem &operator=(const SocketSystem &) = delete;

    // Registers `socket`, or updates its interest when it is already registered.
    // Read interest can be dropped for a peer that closed its write side, so a
    // pending reply cannot make the loop spin on an always-readable descriptor.
    void watch(Socket &socket, int token, bool readable, bool writable);
    void unwatch(Socket &socket) noexcept;

    // Waits for readiness. The returned span stays valid until the next wait().
    std::span<const SocketEvent> wait(int timeout_ms);

    // Reserved token reported for the listening socket.
    static constexpr int listener_token = -1;

  private:
    struct Platform;
    std::unique_ptr<Platform> platform_;
};

class Socket
{
  public:
    Socket();
    ~Socket();

    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;
    Socket(Socket &&other) noexcept;
    Socket &operator=(Socket &&other) noexcept;

    void create_tcp();
    void bind(std::uint16_t port);
    void listen(int backlog);
    Socket accept();
    void set_nonblocking();
    void set_nodelay();

    int receive(char *buffer, std::size_t length);
    int send(const char *data, std::size_t length);
    bool would_block() const;
    bool valid() const;
    void close();

  private:
    friend class SocketSystem;

    std::intptr_t handle_;
};

} // namespace reddish
