#include "resp.hpp"

#include "Database.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <stdexcept>
#include <type_traits>
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

Response execute_command(const Command &command, Database &database)
{
    std::string name = command.name;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character)
                   { return static_cast<char>(std::toupper(character)); });

    if (name == "PING") return Response{SimpleString{"PONG"}};

    if (name == "SET")
    {
        if (command.arguments.size() != 2)
            return Response{ErrorResponse{"ERR wrong number of arguments for 'set' command"}};

        database.set(command.arguments[0], command.arguments[1]);
        return Response{SimpleString{"OK"}};
    }

    if (name == "GET")
    {
        if (command.arguments.size() != 1)
            return Response{ErrorResponse{"ERR wrong number of arguments for 'get' command"}};

        const auto value = database.get(command.arguments[0]);
        if (!value) return Response{Null{}};
        return Response{BulkString{*value}};
    }

    if (name == "DEL")
    {
        if (command.arguments.empty())
            return Response{ErrorResponse{"ERR wrong number of arguments for 'del' command"}};

        std::int64_t deleted = 0;
        for (const auto &key : command.arguments)
            deleted += database.del(key) ? 1 : 0;
        return Response{Integer{deleted}};
    }

    return Response{ErrorResponse{"ERR unknown command '" + command.name + "'"}};
}

std::string encode_response(const Response &response)
{
    std::string encoded;
    std::visit(
        [&](const auto &data)
        {
            using Data = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<Data, SimpleString>)
            {
                encoded += "+";
                encoded += data.value;
                encoded += "\r\n";
            }
            else if constexpr (std::is_same_v<Data, BulkString>)
            {
                encoded += "$";
                encoded += std::to_string(data.value.size());
                encoded += "\r\n";
                encoded += data.value;
                encoded += "\r\n";
            }
            else if constexpr (std::is_same_v<Data, ErrorResponse>)
            {
                encoded += "-";
                encoded += data.message;
                encoded += "\r\n";
            }
            else if constexpr (std::is_same_v<Data, Integer>)
            {
                encoded += ":";
                encoded += std::to_string(data.value);
                encoded += "\r\n";
            }
            else if constexpr (std::is_same_v<Data, Null>)
            {
                encoded += "$-1\r\n";
            }
            else if constexpr (std::is_same_v<Data, ResponseArray>)
            {
                encoded += "*";
                encoded += std::to_string(data.values.size());
                encoded += "\r\n";

                for (const Response &element : data.values)
                    encoded += encode_response(element);
            }
        },
        response.data);

    return encoded;
}
} // namespace reddish
