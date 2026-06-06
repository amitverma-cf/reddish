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

namespace
{

Response response_from_value(const Value &value)
{
    return std::visit(
        [](const auto &data) -> Response
        {
            using Data = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<Data, std::string>)
            {
                return Response{BulkString{data}};
            }
            else if constexpr (std::is_same_v<Data, std::int64_t>)
            {
                return Response{Integer{data}};
            }
            else if constexpr (std::is_same_v<Data, ListPtr>)
            {
                ResponseArray array;
                for (const auto &element : data->values)
                    array.values.push_back(response_from_value(element));
                return Response{std::move(array)};
            }
            else
            {
                ResponseArray array;
                for (const auto &[field, element] : data->fields)
                {
                    array.values.push_back(Response{BulkString{field}});
                    array.values.push_back(response_from_value(element));
                }
                return Response{std::move(array)};
            }
        },
        value);
}

Result<std::int64_t> parse_integer(const std::string &input)
{
    try
    {
        std::size_t consumed = 0;
        const auto value = std::stoll(input, &consumed);
        if (consumed != input.size()) return std::unexpected(Error{ErrorCode::value_not_integer});
        return value;
    }
    catch (const std::exception &)
    {
        return std::unexpected(Error{ErrorCode::value_not_integer});
    }
}

} // namespace

Result<Command> parse_command(std::string_view input)
{
    std::size_t pos = 0;
    auto readline = [&]() -> Result<std::string_view>
    {
        size_t end = input.find("\r\n", pos);
        if (end == std::string::npos) return std::unexpected(Error{ErrorCode::missing_crlf});
        const auto line = input.substr(pos, end - pos);
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
        argc = std::stoul(std::string{argc_result.value()}, &consumed);
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
            len = std::stoul(std::string{len_result.value()}, &consumed);
            if (consumed != len_result.value().size())
                return std::unexpected(Error{ErrorCode::invalid_bulk_string_length});
        }
        catch (const std::exception &)
        {
            return std::unexpected(Error{ErrorCode::invalid_bulk_string_length});
        }
        if (pos > input.size() || input.size() - pos < 2 || len > input.size() - pos - 2)
            return std::unexpected(Error{ErrorCode::incomplete_bulk_string_data});
        const auto value = input.substr(pos, len);
        if (i == 0) cmd.name = value;
        else cmd.arguments.emplace_back(value);
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

Response execute_command(const Command &command, Database &database, const ServerStats &stats)
{
    std::string name{command.name};
    std::vector<std::string> arguments;
    arguments.reserve(command.arguments.size());
    for (const auto argument : command.arguments)
        arguments.emplace_back(argument);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character)
                   { return static_cast<char>(std::toupper(character)); });

    if (name == "PING") return Response{SimpleString{"PONG"}};

    if (name == "INFO")
    {
        if (arguments.size() > 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'info' command"}};

        const bool memory_only = !arguments.empty() && arguments[0] == "memory";
        if (!arguments.empty() && !memory_only)
            return Response{
                ErrorResponse{std::string(error_message(ErrorCode::unsupported_info_section))}};

        const auto database_stats = database.stats();
        std::string response;
        if (!memory_only)
        {
            response += "# Server\r\n";
            response += "uptime_seconds:" + std::to_string(stats.uptime_seconds) + "\r\n";
            response += "connected_clients:" + std::to_string(stats.connected_clients) + "\r\n";
            response +=
                "input_buffer_disconnects:" + std::to_string(stats.input_buffer_disconnects) +
                "\r\n";
            response +=
                "output_buffer_disconnects:" + std::to_string(stats.output_buffer_disconnects) +
                "\r\n";
            response += "# Keyspace\r\n";
            response += "keys:" + std::to_string(database_stats.key_count) + "\r\n";
            response += "evictions:" + std::to_string(database_stats.evictions) + "\r\n";
            response += "expired_keys:" + std::to_string(database_stats.expired_keys) + "\r\n";
        }
        response += "# Memory\r\n";
        response +=
            "used_memory_bytes:" + std::to_string(database_stats.approximate_memory_bytes) + "\r\n";
        response += "snapshot_bytes:" + std::to_string(database_stats.snapshot_bytes) + "\r\n";
        response +=
            "last_dump_unix_ms:" + std::to_string(database_stats.last_dump_unix_ms) + "\r\n";
        return Response{BulkString{std::move(response)}};
    }

    if (name == "SET")
    {
        if (arguments.size() != 2)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'set' command"}};

        database.set(arguments[0], arguments[1]);
        return Response{SimpleString{"OK"}};
    }

    if (name == "GET")
    {
        if (arguments.size() != 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'get' command"}};

        const auto value = database.get(command.arguments[0]);
        if (!value)
            return Response{ErrorResponse{std::string(error_message(value.error().code()))}};
        if (!*value) return Response{Null{}};
        return Response{BulkString{**value}};
    }

    if (name == "INCR")
    {
        if (arguments.size() != 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'incr' command"}};

        const auto value = database.increment(arguments[0]);
        if (!value)
            return Response{ErrorResponse{std::string(error_message(value.error().code()))}};
        return Response{Integer{*value}};
    }

    if (name == "LPUSH" || name == "RPUSH")
    {
        if (arguments.size() < 2)
            return Response{
                ErrorResponse{std::string(error_message(ErrorCode::wrong_argument_count)) + " '" +
                              std::string(name.begin(), name.end()) + "' command"}};

        Result<std::int64_t> length = std::int64_t{0};
        for (std::size_t index = 1; index < arguments.size(); ++index)
        {
            length = name == "LPUSH" ? database.push_left(arguments[0], arguments[index])
                                     : database.push_right(arguments[0], arguments[index]);
            if (!length)
                return Response{ErrorResponse{std::string(error_message(length.error().code()))}};
        }
        return Response{Integer{*length}};
    }

    if (name == "LPOP" || name == "RPOP")
    {
        if (arguments.size() != 1)
            return Response{
                ErrorResponse{std::string(error_message(ErrorCode::wrong_argument_count)) + " '" +
                              std::string(name.begin(), name.end()) + "' command"}};

        const auto value =
            name == "LPOP" ? database.pop_left(arguments[0]) : database.pop_right(arguments[0]);
        if (!value)
            return Response{ErrorResponse{std::string(error_message(value.error().code()))}};
        if (!*value) return Response{Null{}};
        return response_from_value(**value);
    }

    if (name == "LLEN")
    {
        if (arguments.size() != 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'llen' command"}};

        const auto length = database.list_length(arguments[0]);
        if (!length)
            return Response{ErrorResponse{std::string(error_message(length.error().code()))}};
        return Response{Integer{*length}};
    }

    if (name == "HSET")
    {
        if (arguments.size() < 3 || arguments.size() % 2 == 0)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'hset' command"}};

        std::int64_t added = 0;
        for (std::size_t index = 1; index < arguments.size(); index += 2)
        {
            const auto result =
                database.hash_set(arguments[0], arguments[index], arguments[index + 1]);
            if (!result)
                return Response{ErrorResponse{std::string(error_message(result.error().code()))}};
            added += *result;
        }
        return Response{Integer{added}};
    }

    if (name == "HGET")
    {
        if (arguments.size() != 2)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'hget' command"}};

        const auto value = database.hash_get(arguments[0], arguments[1]);
        if (!value)
            return Response{ErrorResponse{std::string(error_message(value.error().code()))}};
        if (!*value) return Response{Null{}};
        return response_from_value(**value);
    }

    if (name == "HDEL")
    {
        if (arguments.size() != 2)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'hdel' command"}};

        const auto removed = database.hash_del(arguments[0], arguments[1]);
        if (!removed)
            return Response{ErrorResponse{std::string(error_message(removed.error().code()))}};
        return Response{Integer{*removed ? 1 : 0}};
    }

    if (name == "HLEN")
    {
        if (arguments.size() != 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'hlen' command"}};

        const auto length = database.hash_length(arguments[0]);
        if (!length)
            return Response{ErrorResponse{std::string(error_message(length.error().code()))}};
        return Response{Integer{*length}};
    }

    if (name == "EXPIRE")
    {
        if (arguments.size() != 2)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'expire' command"}};

        const auto seconds = parse_integer(arguments[1]);
        if (!seconds)
            return Response{ErrorResponse{std::string(error_message(seconds.error().code()))}};
        return Response{Integer{database.expire(arguments[0], *seconds) ? 1 : 0}};
    }

    if (name == "TTL")
    {
        if (arguments.size() != 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'ttl' command"}};

        return Response{Integer{database.ttl(arguments[0])}};
    }

    if (name == "DEL")
    {
        if (arguments.empty())
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'del' command"}};

        std::int64_t deleted = 0;
        for (const auto &key : arguments)
            deleted += database.del(key) ? 1 : 0;
        return Response{Integer{deleted}};
    }

    return Response{ErrorResponse{std::string(error_message(ErrorCode::unknown_command)) + " '" +
                                  std::string{command.name} + "'"}};
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
