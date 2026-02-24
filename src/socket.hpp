#pragma once

#include <cstddef>
#include <cstdint>

namespace reddish
{

class SocketSystem
{
  public:
    SocketSystem();
    ~SocketSystem();
    SocketSystem(const SocketSystem &) = delete;
    SocketSystem &operator=(const SocketSystem &) = delete;
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

    int receive(char *buffer, std::size_t length);
    int send(const char *data, std::size_t length);
    bool would_block() const;
    bool valid() const;
    void close();
    static void wait(int milliseconds);

  private:
    std::intptr_t handle_;
};

} // namespace reddish
