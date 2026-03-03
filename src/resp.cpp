#include "resp.hpp"
#include "database.hpp"
#include "error.hpp"
#include <algorithm>
#include <cctype>
#include <exception>
#include <type_traits>
#include <utility>

namespace reddish
{

Result<Command> parse_command(const std::string &input)
{
    std::size_t pos = 0;
    auto readline = [&]() -> Result<std::string>
    {
        size_t end = input.find("\r\n", pos);
        if (end == std::string::npos) return std::unexpected(Error{ErrorCode::missing_crlf});
        std::string line = input.substr(pos, end - pos);
        pos = end + 2;
        return line;
    };
    if (input.empty()) return std::unexpected(Error{ErrorCode::empty_resp_input});
    if (input[0] != '*') return std::unexpected(Error{ErrorCode::expected_resp_array_prefix});
    pos++;
    auto argc_result = readline();
    if (!argc_result.has_value()) return std::unexpected(argc_result.error());
    size_t argc;
    try
    {
        size_t consumed = 0;
        argc = std::stoul(argc_result.value(), &consumed);
        if (consumed != argc_result.value().size())
            return std::unexpected(Error{ErrorCode::invalid_resp_array_length});
    }
    catch (const std::exception &)
    {
        return std::unexpected(Error{ErrorCode::invalid_resp_array_length});
    }
    if (argc == 0) return std::unexpected(Error{ErrorCode::empty_resp_array});
    Command cmd;
    for (size_t i = 0; i < argc; i++)
    {
        if (pos >= input.size())
            return std::unexpected(Error{ErrorCode::missing_bulk_string_prefix});
        if (input[pos++] != '$')
            return std::unexpected(Error{ErrorCode::expected_bulk_string_prefix});
        auto len_result = readline();
        if (!len_result.has_value()) return std::unexpected(len_result.error());
        size_t len;
        try
        {
            size_t consumed = 0;
            len = std::stoul(len_result.value(), &consumed);
            if (consumed != len_result.value().size())
                return std::unexpected(Error{ErrorCode::invalid_bulk_string_length});
        }
        catch (const std::exception &)
        {
            return std::unexpected(Error{ErrorCode::invalid_bulk_string_length});
        }
        if (pos > input.size() || input.size() - pos < 2 || len > input.size() - pos - 2)
            return std::unexpected(Error{ErrorCode::incomplete_bulk_string_data});
        std::string value(input.data() + pos, len);
        if (i == 0) cmd.name = value;
        else cmd.arguments.emplace_back(std::move(value));
        pos += len;
        if (input.size() - pos < 2)
            return std::unexpected(Error{ErrorCode::missing_bulk_string_terminator});
        if (input.compare(pos, 2, "\r\n") != 0)
            return std::unexpected(Error{ErrorCode::missing_bulk_string_terminator});
        pos += 2;
    }
    cmd.bytes_consumed = pos;
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
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'set' command"}};

        database.set(command.arguments[0], command.arguments[1]);
        return Response{SimpleString{"OK"}};
    }

    if (name == "GET")
    {
        if (command.arguments.size() != 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'get' command"}};

        const auto value = database.get(command.arguments[0]);
        if (!value) return Response{Null{}};
        return Response{BulkString{*value}};
    }

    if (name == "INCR")
    {
        if (command.arguments.size() != 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'incr' command"}};

        const auto value = database.increment(command.arguments[0]);
        if (!value)
            return Response{ErrorResponse{std::string(error_message(value.error().code()))}};
        return Response{Integer{*value}};
    }

    if (name == "DEL")
    {
        if (command.arguments.empty())
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'del' command"}};

        std::int64_t deleted = 0;
        for (const auto &key : command.arguments)
            deleted += database.del(key) ? 1 : 0;
        return Response{Integer{deleted}};
    }

    return Response{ErrorResponse{std::string(error_message(ErrorCode::unknown_command)) + " '" +
                                  command.name + "'"}};
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
