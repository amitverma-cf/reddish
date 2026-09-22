#include "client.hpp"
#include <charconv>

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
    if (!sent)
    {
        close();
        return std::unexpected(sent.error());
    }

    std::string response;
    std::size_t position = 0;
    std::size_t pending = 1;
    constexpr std::size_t limit = 16 * 1024 * 1024;
    while (pending != 0)
    {
        const auto end = response.find("\r\n", position);
        bool incomplete = end == std::string::npos;
        if (!incomplete)
        {
            const char type = response[position];
            std::int64_t length = 0;
            if (type == '$' || type == '*')
            {
                const auto first = response.data() + position + 1;
                const auto last = response.data() + end;
                const auto [ptr, ec] = std::from_chars(first, last, length);
                if (ec != std::errc{} || ptr != last || length < -1 ||
                    length > static_cast<std::int64_t>(limit))
                {
                    close();
                    return std::unexpected("Invalid RESP length");
                }
            }
            else if (type != '+' && type != '-' && type != ':')
            {
                close();
                return std::unexpected("Invalid RESP reply type");
            }
            std::size_t next = end + 2;
            if (type == '$' && length >= 0)
            {
                next += static_cast<std::size_t>(length) + 2;
                incomplete = response.size() < next;
                if (!incomplete && response.compare(next - 2, 2, "\r\n") != 0)
                {
                    close();
                    return std::unexpected("Invalid RESP bulk terminator");
                }
            }
            if (!incomplete)
            {
                position = next;
                --pending;
                if (type == '*' && length > 0) pending += static_cast<std::size_t>(length);
                if (pending > limit)
                {
                    close();
                    return std::unexpected("RESP reply too large");
                }
                continue;
            }
        }
        char buffer[4096];
        const auto count = socket_.receive(buffer, sizeof(buffer));
        if (!count)
        {
            close();
            return std::unexpected(count.error());
        }
        if (response.size() + static_cast<std::size_t>(*count) > limit)
        {
            close();
            return std::unexpected("RESP reply too large");
        }
        response.append(buffer, static_cast<std::size_t>(*count));
    }
    if (position != response.size())
    {
        close();
        return std::unexpected("Unexpected data after RESP reply");
    }
    return response;
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
