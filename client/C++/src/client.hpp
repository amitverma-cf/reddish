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
    std::expected<std::string, std::string> ping();
    std::expected<std::string, std::string> set(std::string_view key, std::string_view value);
    std::expected<std::string, std::string> get(std::string_view key);
    std::expected<std::string, std::string> increment(std::string_view key);
    std::expected<std::string, std::string> del(std::string_view key);
    std::expected<std::string, std::string> expire(std::string_view key, std::int64_t seconds);
    std::expected<std::string, std::string> ttl(std::string_view key);
    std::expected<std::string, std::string> lpush(std::string_view key, std::string_view value);
    std::expected<std::string, std::string> rpush(std::string_view key, std::string_view value);
    std::expected<std::string, std::string> lpop(std::string_view key);
    std::expected<std::string, std::string> rpop(std::string_view key);
    std::expected<std::string, std::string> llen(std::string_view key);
    std::expected<std::string, std::string> hset(std::string_view key, std::string_view field,
                                                 std::string_view value);
    std::expected<std::string, std::string> hget(std::string_view key, std::string_view field);
    std::expected<std::string, std::string> hdel(std::string_view key, std::string_view field);
    std::expected<std::string, std::string> hlen(std::string_view key);
    std::expected<std::string, std::string> info(std::string_view section = {});
    void close();

  private:
    std::string host_;
    std::uint16_t port_;
    SocketSystem socket_system_;
    Socket socket_;
};

} // namespace reddish::client
