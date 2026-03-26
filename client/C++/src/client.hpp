#pragma once

#include "socket.hpp"
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace reddish::client
{

class Client
{
  public:
    Client(std::string host = "127.0.0.1", std::uint16_t port = 6379);
    ~Client();

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    std::expected<void, std::string> connect();
    std::expected<std::string, std::string> execute(const std::vector<std::string_view> &command);
    void close();

  private:
    std::string host_;
    std::uint16_t port_;
    SocketSystem socket_system_;
    Socket socket_;
};

} // namespace reddish::client
