#include "resp.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace reddish
{

std::expected<Command, std::string> parse_command(const std::string &input)
{
    if (input.empty()) return std::unexpected("Empty RESP input");

    std::size_t position = 0;
    auto read_line = [&]() -> std::expected<std::string, std::string>
    {
        const std::size_t end = input.find("\r\n", position);
        if (end == std::string::npos) return std::unexpected("Missing CRLF");

        std::string line = input.substr(position, end - position);
        position = end + 2;
        return line;
    };

    if (input[position++] != '*') return std::unexpected("Expected RESP array prefix '*'");

    const auto count_line = read_line();
    if (!count_line) return std::unexpected(count_line.error());

    std::size_t count = 0;
    try
    {
        std::size_t consumed = 0;
        count = std::stoul(*count_line, &consumed);
        if (consumed != count_line->size()) return std::unexpected("Invalid RESP array length");
    }
    catch (const std::exception &)
    {
        return std::unexpected("Invalid RESP array length");
    }

    if (count == 0) return std::unexpected("RESP array contains no command");

    Command command;
    for (std::size_t index = 0; index < count; ++index)
    {
        if (position >= input.size() || input[position++] != '$')
            return std::unexpected("Expected RESP bulk string prefix '$'");

        const auto length_line = read_line();
        if (!length_line) return std::unexpected(length_line.error());

        std::size_t length = 0;
        try
        {
            std::size_t consumed = 0;
            length = std::stoul(*length_line, &consumed);
            if (consumed != length_line->size())
                return std::unexpected("Invalid RESP bulk string length");
        }
        catch (const std::exception &)
        {
            return std::unexpected("Invalid RESP bulk string length");
        }

        if (input.size() - position < length + 2)
            return std::unexpected("RESP bulk string data is incomplete");

        std::string part(input.data() + position, length);
        position += length;

        if (input.compare(position, 2, "\r\n") != 0)
            return std::unexpected("RESP bulk string is missing trailing CRLF");
        position += 2;

        if (index == 0) command.name = std::move(part);
        else command.arguments.emplace_back(std::move(part));
    }

    if (position != input.size()) return std::unexpected("Unexpected trailing RESP data");

    return command;
}

} // namespace reddish
