#include "resp.hpp"
#include "database.hpp"
#include "error.hpp"
#include <charconv>
#include <limits>
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

template <typename T> Result<T> parse_number(std::string_view input, ErrorCode error_code)
{
    T value = 0;
    const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);
    if (error != std::errc{} || end != input.data() + input.size())
        return std::unexpected(Error{error_code});
    return value;
}

bool command_is(std::string_view command, std::string_view expected)
{
    if (command.size() != expected.size()) return false;

    for (std::size_t index = 0; index < command.size(); ++index)
    {
        const auto character = command[index];
        if (character != expected[index] && character != expected[index] + ('a' - 'A'))
            return false;
    }
    return true;
}

void append_decimal(std::int64_t value, std::string &output)
{
    char buffer[std::numeric_limits<std::int64_t>::digits10 + 3];
    const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (error == std::errc{}) output.append(buffer, end);
}

std::size_t decimal_size(std::int64_t value)
{
    char buffer[std::numeric_limits<std::int64_t>::digits10 + 3];
    const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return error == std::errc{} ? static_cast<std::size_t>(end - buffer) : 0;
}

std::size_t encoded_response_size_impl(const Response &response)
{
    return std::visit(
        [](const auto &data) -> std::size_t
        {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, SimpleString>)
            {
                return 3 + data.value.size();
            }
            else if constexpr (std::is_same_v<T, ErrorResponse>)
            {
                return 3 + data.message.size();
            }
            else if constexpr (std::is_same_v<T, Integer>)
            {
                return 3 + decimal_size(data.value);
            }
            else if constexpr (std::is_same_v<T, BulkString> || std::is_same_v<T, BulkStringView>)
            {
                return 5 + decimal_size(static_cast<std::int64_t>(data.value.size())) +
                       data.value.size();
            }
            else if constexpr (std::is_same_v<T, Null>)
            {
                return 5;
            }
            else
            {
                std::size_t size = 3 + decimal_size(static_cast<std::int64_t>(data.values.size()));
                for (const auto &element : data.values)
                    size += encoded_response_size_impl(element);
                return size;
            }
        },
        response.data);
}

void append_encoded_response_impl(const Response &response, std::string &output)
{
    std::visit(
        [&](const auto &data)
        {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, SimpleString>)
            {
                output.push_back('+');
                output += data.value;
                output.append("\r\n", 2);
            }
            else if constexpr (std::is_same_v<T, ErrorResponse>)
            {
                output.push_back('-');
                output += data.message;
                output.append("\r\n", 2);
            }
            else if constexpr (std::is_same_v<T, Integer>)
            {
                output.push_back(':');
                append_decimal(data.value, output);
                output.append("\r\n", 2);
            }
            else if constexpr (std::is_same_v<T, BulkString> || std::is_same_v<T, BulkStringView>)
            {
                output.push_back('$');
                append_decimal(static_cast<std::int64_t>(data.value.size()), output);
                output.append("\r\n", 2);
                output += data.value;
                output.append("\r\n", 2);
            }
            else if constexpr (std::is_same_v<T, Null>)
            {
                output += "$-1\r\n";
            }
            else if constexpr (std::is_same_v<T, ResponseArray>)
            {
                output.push_back('*');
                append_decimal(static_cast<std::int64_t>(data.values.size()), output);
                output.append("\r\n", 2);
                for (const auto &element : data.values)
                    append_encoded_response_impl(element, output);
            }
        },
        response.data);
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
    const auto argc =
        parse_number<std::size_t>(argc_result.value(), ErrorCode::invalid_resp_array_length);
    if (!argc) return std::unexpected(argc.error());
    if (*argc == 0) return std::unexpected(Error{ErrorCode::empty_resp_array});
    Command cmd;
    cmd.arguments.reserve(*argc - 1);
    for (std::size_t i = 0; i < *argc; i++)
    {
        if (pos >= input.size())
            return std::unexpected(Error{ErrorCode::missing_bulk_string_prefix});
        if (input[pos++] != '$')
            return std::unexpected(Error{ErrorCode::expected_bulk_string_prefix});
        auto len_result = readline();
        if (!len_result.has_value()) return std::unexpected(len_result.error());
        const auto length =
            parse_number<std::size_t>(len_result.value(), ErrorCode::invalid_bulk_string_length);
        if (!length) return std::unexpected(length.error());
        if (pos > input.size() || input.size() - pos < 2 || *length > input.size() - pos - 2)
            return std::unexpected(Error{ErrorCode::incomplete_bulk_string_data});
        const auto value = input.substr(pos, *length);
        if (i == 0) cmd.name = value;
        else cmd.arguments.emplace_back(value);
        pos += *length;
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
    const auto &arguments = command.arguments;

    if (command_is(command.name, "PING")) return Response{SimpleString{"PONG"}};

    if (command_is(command.name, "INFO"))
    {
        if (arguments.size() > 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'info' command"}};

        const bool memory_only = !arguments.empty() && arguments[0] == "memory";
        if (!arguments.empty() && !memory_only)
            return Response{
                ErrorResponse{std::string(error_message(ErrorCode::unsupported_info_section))}};

        const auto database_stats = database.stats();
        const auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::steady_clock::now() - stats.started_at)
                                        .count();
        std::string response;
        if (!memory_only)
        {
            response += "# Server\r\n";
            response += "uptime_seconds:" + std::to_string(uptime_seconds) + "\r\n";
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

    if (command_is(command.name, "SET"))
    {
        if (arguments.size() != 2)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'set' command"}};

        database.set(arguments[0], arguments[1]);
        return Response{SimpleString{"OK"}};
    }

    if (command_is(command.name, "GET"))
    {
        if (arguments.size() != 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'get' command"}};

        const auto value = database.get_value(command.arguments[0]);
        if (!value)
            return Response{ErrorResponse{std::string(error_message(value.error().code()))}};
        if (!*value) return Response{Null{}};
        if (const auto *text = std::get_if<std::string>(&value->value().get()))
            return Response{BulkStringView{*text}};
        if (const auto *integer = std::get_if<std::int64_t>(&value->value().get()))
            return Response{BulkString{std::to_string(*integer)}};
        return Response{ErrorResponse{std::string(error_message(ErrorCode::wrong_type))}};
    }

    if (command_is(command.name, "INCR"))
    {
        if (arguments.size() != 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'incr' command"}};

        const auto value = database.increment(arguments[0]);
        if (!value)
            return Response{ErrorResponse{std::string(error_message(value.error().code()))}};
        return Response{Integer{*value}};
    }

    if (command_is(command.name, "LPUSH") || command_is(command.name, "RPUSH"))
    {
        if (arguments.size() < 2)
            return Response{
                ErrorResponse{std::string(error_message(ErrorCode::wrong_argument_count)) + " '" +
                              std::string(command.name) + "' command"}};

        Result<std::int64_t> length = std::int64_t{0};
        for (std::size_t index = 1; index < arguments.size(); ++index)
        {
            length = command_is(command.name, "LPUSH")
                         ? database.push_left(arguments[0], Value{std::string{arguments[index]}})
                         : database.push_right(arguments[0], Value{std::string{arguments[index]}});
            if (!length)
                return Response{ErrorResponse{std::string(error_message(length.error().code()))}};
        }
        return Response{Integer{*length}};
    }

    if (command_is(command.name, "LPOP") || command_is(command.name, "RPOP"))
    {
        if (arguments.size() != 1)
            return Response{
                ErrorResponse{std::string(error_message(ErrorCode::wrong_argument_count)) + " '" +
                              std::string(command.name) + "' command"}};

        const auto value = command_is(command.name, "LPOP") ? database.pop_left(arguments[0])
                                                            : database.pop_right(arguments[0]);
        if (!value)
            return Response{ErrorResponse{std::string(error_message(value.error().code()))}};
        if (!*value) return Response{Null{}};
        return response_from_value(**value);
    }

    if (command_is(command.name, "LLEN"))
    {
        if (arguments.size() != 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'llen' command"}};

        const auto length = database.list_length(arguments[0]);
        if (!length)
            return Response{ErrorResponse{std::string(error_message(length.error().code()))}};
        return Response{Integer{*length}};
    }

    if (command_is(command.name, "HSET"))
    {
        if (arguments.size() < 3 || arguments.size() % 2 == 0)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'hset' command"}};

        std::int64_t added = 0;
        for (std::size_t index = 1; index < arguments.size(); index += 2)
        {
            const auto result = database.hash_set(arguments[0], arguments[index],
                                                  Value{std::string{arguments[index + 1]}});
            if (!result)
                return Response{ErrorResponse{std::string(error_message(result.error().code()))}};
            added += *result;
        }
        return Response{Integer{added}};
    }

    if (command_is(command.name, "HGET"))
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

    if (command_is(command.name, "HDEL"))
    {
        if (arguments.size() != 2)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'hdel' command"}};

        const auto removed = database.hash_del(arguments[0], arguments[1]);
        if (!removed)
            return Response{ErrorResponse{std::string(error_message(removed.error().code()))}};
        return Response{Integer{*removed ? 1 : 0}};
    }

    if (command_is(command.name, "HLEN"))
    {
        if (arguments.size() != 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'hlen' command"}};

        const auto length = database.hash_length(arguments[0]);
        if (!length)
            return Response{ErrorResponse{std::string(error_message(length.error().code()))}};
        return Response{Integer{*length}};
    }

    if (command_is(command.name, "EXPIRE"))
    {
        if (arguments.size() != 2)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'expire' command"}};

        const auto seconds = parse_number<std::int64_t>(arguments[1], ErrorCode::value_not_integer);
        if (!seconds)
            return Response{ErrorResponse{std::string(error_message(seconds.error().code()))}};
        return Response{Integer{database.expire(arguments[0], *seconds) ? 1 : 0}};
    }

    if (command_is(command.name, "TTL"))
    {
        if (arguments.size() != 1)
            return Response{ErrorResponse{
                std::string(error_message(ErrorCode::wrong_argument_count)) + " 'ttl' command"}};

        return Response{Integer{database.ttl(arguments[0])}};
    }

    if (command_is(command.name, "DEL"))
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
    std::string output;
    output.reserve(encoded_response_size_impl(response));
    append_encoded_response_impl(response, output);
    return output;
}

std::size_t encoded_response_size(const Response &response)
{
    return encoded_response_size_impl(response);
}

void append_encoded_response(const Response &response, std::string &output)
{
    output.reserve(output.size() + encoded_response_size_impl(response));
    append_encoded_response_impl(response, output);
}

} // namespace reddish
