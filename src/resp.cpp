#include "resp.hpp"
#include "database.hpp"
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
    std::size_t pos = 0;
    auto readline = [&]() -> std::expected<std::string, std::string>
    {
        size_t end = input.find("\r\n", pos);
        if (end == std::string::npos) return std::unexpected("Missing CRLF");
        std::string line = input.substr(pos, end - pos);
        pos = end + 2;
        return line;
    };
    if (input.empty()) return std::unexpected("Empty RESP input");
    if (input[0] != '*') return std::unexpected("Expected RESP array prefix '*'");
    pos++;
    auto argc_result = readline();
    if (!argc_result.has_value()) return std::unexpected(argc_result.error());
    size_t argc;
    try
    {
        size_t consumed = 0;
        argc = std::stoul(argc_result.value(), &consumed);
        if (consumed != argc_result.value().size())
            throw std::invalid_argument("invalid array length");
    }
    catch (const std::exception &)
    {
        return std::unexpected("Invalid RESP array length");
    }
    if (argc == 0) return std::unexpected("RESP array contains no command");
    Command cmd;
    for (size_t i = 0; i < argc; i++)
    {
        if (pos >= input.size()) return std::unexpected("Missing bulk string prefix");
        if (input[pos++] != '$') return std::unexpected("Expected bulk string prefix '$'");
        auto len_result = readline();
        if (!len_result.has_value()) return std::unexpected(len_result.error());
        size_t len;
        try
        {
            size_t consumed = 0;
            len = std::stoul(len_result.value(), &consumed);
            if (consumed != len_result.value().size())
                throw std::invalid_argument("invalid bulk string length");
        }
        catch (const std::exception &)
        {
            return std::unexpected("Invalid RESP bulk string length");
        }
        if (pos > input.size() || input.size() - pos < 2 || len > input.size() - pos - 2)
            return std::unexpected("RESP bulk string data is incomplete");
        std::string value(input.data() + pos, len);
        if (i == 0) cmd.name = value;
        else cmd.arguments.emplace_back(std::move(value));
        pos += len;
        if (input.size() - pos < 2)
            return std::unexpected("RESP bulk string is missing trailing CRLF");
        if (input.compare(pos, 2, "\r\n") != 0)
            return std::unexpected("RESP bulk string is missing trailing CRLF");
        pos += 2;
    }
    if (pos != input.size()) return std::unexpected("Unexpected trailing RESP data");
    return cmd;
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
    std::string res;
    std::visit(
        [&](const auto &data)
        {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, SimpleString>)
            {
                res += "+";
                res += data.value;
                res += "\r\n";
            }
            else if constexpr (std::is_same_v<T, ErrorResponse>)
            {
                res += "-";
                res += data.message;
                res += "\r\n";
            }
            else if constexpr (std::is_same_v<T, Integer>)
            {
                res += ":";
                res += std::to_string(data.value);
                res += "\r\n";
            }
            else if constexpr (std::is_same_v<T, BulkString>)
            {
                res += "$";
                res += std::to_string(data.value.size());
                res += "\r\n";
                res += data.value;
                res += "\r\n";
            }
            else if constexpr (std::is_same_v<T, Null>)
            {
                res += "$-1\r\n";
            }
            else if constexpr (std::is_same_v<T, ResponseArray>)
            {
                res += "*";
                res += std::to_string(data.values.size());
                res += "\r\n";

                for (const auto &element : data.values)
                {
                    res += encode_response(element);
                }
            }
        },
        response.data);
    return res;
}

} // namespace reddish
