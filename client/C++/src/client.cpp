#include "client.hpp"

namespace reddish::client
{

Client::Client(std::string host, std::uint16_t port) : host_(std::move(host)), port_(port) {}

Client::~Client()
{
    close();
}

std::expected<void, std::string> Client::connect()
{
    return socket_.connect(host_, port_);
}

std::expected<std::string, std::string> Client::execute(
    const std::vector<std::string_view> &command)
{
    std::string request = "*" + std::to_string(command.size()) + "\r\n";
    for (const auto part : command)
        request += "$" + std::to_string(part.size()) + "\r\n" + std::string(part) + "\r\n";

    const auto sent = socket_.send(request.data(), request.size());
    if (!sent) return std::unexpected(sent.error());

    char buffer[4096];
    const auto count = socket_.receive(buffer, sizeof(buffer));
    if (!count) return std::unexpected(count.error());
    return std::string(buffer, *count);
}

std::expected<std::string, std::string> Client::ping()
{
    return execute({"PING"});
}
std::expected<std::string, std::string> Client::set(std::string_view key, std::string_view value)
{
    return execute({"SET", key, value});
}
std::expected<std::string, std::string> Client::get(std::string_view key)
{
    return execute({"GET", key});
}
std::expected<std::string, std::string> Client::increment(std::string_view key)
{
    return execute({"INCR", key});
}
std::expected<std::string, std::string> Client::del(std::string_view key)
{
    return execute({"DEL", key});
}
std::expected<std::string, std::string> Client::expire(std::string_view key, std::int64_t seconds)
{
    const auto value = std::to_string(seconds);
    return execute({"EXPIRE", key, value});
}
std::expected<std::string, std::string> Client::ttl(std::string_view key)
{
    return execute({"TTL", key});
}
std::expected<std::string, std::string> Client::lpush(std::string_view key, std::string_view value)
{
    return execute({"LPUSH", key, value});
}
std::expected<std::string, std::string> Client::rpush(std::string_view key, std::string_view value)
{
    return execute({"RPUSH", key, value});
}
std::expected<std::string, std::string> Client::lpop(std::string_view key)
{
    return execute({"LPOP", key});
}
std::expected<std::string, std::string> Client::rpop(std::string_view key)
{
    return execute({"RPOP", key});
}
std::expected<std::string, std::string> Client::llen(std::string_view key)
{
    return execute({"LLEN", key});
}
std::expected<std::string, std::string> Client::hset(std::string_view key, std::string_view field,
                                                     std::string_view value)
{
    return execute({"HSET", key, field, value});
}
std::expected<std::string, std::string> Client::hget(std::string_view key, std::string_view field)
{
    return execute({"HGET", key, field});
}
std::expected<std::string, std::string> Client::hdel(std::string_view key, std::string_view field)
{
    return execute({"HDEL", key, field});
}
std::expected<std::string, std::string> Client::hlen(std::string_view key)
{
    return execute({"HLEN", key});
}
std::expected<std::string, std::string> Client::info(std::string_view section)
{
    return section.empty() ? execute({"INFO"}) : execute({"INFO", section});
}

void Client::close()
{
    socket_.close();
}

} // namespace reddish::client
