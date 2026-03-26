#pragma once

#include <cstdint>
#include <expected>
#include <string>

namespace reddish::client
{

class SocketSystem
{
  public:
    SocketSystem();
    ~SocketSystem();
};

class Socket
{
  public:
    ~Socket();
    std::expected<void, std::string> connect(const std::string &host, std::uint16_t port);
    std::expected<int, std::string> send(const char *data, std::size_t size);
    std::expected<int, std::string> receive(char *data, std::size_t size);
    void close();

  private:
    std::intptr_t handle_ = -1;
};

} // namespace reddish::client
