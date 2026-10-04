#include "resp.hpp"
#include "error.hpp"

#include <charconv>
#include <cstring>
#include <functional>
#include <limits>

namespace reddish
{

namespace
{

constexpr std::uint64_t tag_of(const char *text) noexcept
{
    std::uint64_t tag = 0;
    std::size_t length = 0;
    while (text[length] != '\0' && length < 8)
        ++length;
    for (std::size_t index = 0; index < length; ++index)
        tag |= static_cast<std::uint64_t>(static_cast<unsigned char>(text[index]) | 0x20U)
               << (index * 8);
    return tag | (static_cast<std::uint64_t>(length) << 56);
}

std::uint64_t tag_of(std::string_view name) noexcept
{
    const std::size_t length = name.size() < 8 ? name.size() : 8;
    std::uint64_t tag = 0;
    for (std::size_t index = 0; index < length; ++index)
        tag |= static_cast<std::uint64_t>(static_cast<unsigned char>(name[index]) | 0x20U)
               << (index * 8);
    return tag | (static_cast<std::uint64_t>(name.size() & 0xFFU) << 56);
}

constexpr std::size_t max_reply_bytes = 1024 * 1024;
void require_reply_space(const std::string &output, std::size_t bytes)
{
    if (bytes > max_reply_bytes || output.size() > max_reply_bytes - bytes)
        throw Error{ErrorCode::request_too_large};
}

void append_decimal(std::int64_t value, std::string &output)
{
    char buffer[24];
    const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (error == std::errc{}) output.append(buffer, end);
}

void append_wrong_arguments(std::string &output, std::string_view name)
{
    append_error(output, std::string(error_message(ErrorCode::wrong_argument_count)) + " '" +
                             std::string{name} + "' command");
}

void append_bulk_view(std::string &output, std::string_view text)
{
    if (text.size() > max_reply_bytes - 32) throw Error{ErrorCode::request_too_large};
    require_reply_space(output, text.size() + 32);
    char header[32];
    header[0] = '$';
    auto [end, error] = std::to_chars(header + 1, header + sizeof(header) - 2, text.size());
    *end++ = '\r';
    *end++ = '\n';
    const auto header_size = static_cast<std::size_t>(end - header);
    const auto offset = output.size();
    const auto size = offset + header_size + text.size() + 2;

    output.resize_and_overwrite(size,
                                [&](char *buffer, std::size_t) noexcept
                                {
                                    std::memcpy(buffer + offset, header, header_size);
                                    if (!text.empty())
                                        std::memcpy(buffer + offset + header_size, text.data(),
                                                    text.size());
                                    buffer[size - 2] = '\r';
                                    buffer[size - 1] = '\n';
                                    return size;
                                });
}

void append_info(Database &database, const ServerStats &stats, bool memory_only,
                 std::string &output)
{
    std::string response;
    response.reserve(512);

    if (!memory_only)
    {
        const auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::steady_clock::now() - stats.started_at)
                                        .count();
        const auto database_stats = database.stats();

        response += "# Server\r\n";
        response += "uptime_seconds:" + std::to_string(uptime_seconds) + "\r\n";
        response += "connected_clients:" + std::to_string(stats.connected_clients) + "\r\n";
        response +=
            "input_buffer_disconnects:" + std::to_string(stats.input_buffer_disconnects) + "\r\n";
        response +=
            "output_buffer_disconnects:" + std::to_string(stats.output_buffer_disconnects) + "\r\n";
        response += "# Keyspace\r\n";
        response += "keys:" + std::to_string(database_stats.key_count) + "\r\n";
        response += "evictions:" + std::to_string(database_stats.evictions) + "\r\n";
        response += "expired_keys:" + std::to_string(database_stats.expired_keys) + "\r\n";
        response += "# Memory\r\n";
        response +=
            "used_memory_bytes:" + std::to_string(database_stats.approximate_memory_bytes) + "\r\n";
        response += "wal_bytes:" + std::to_string(database_stats.wal_bytes) + "\r\n";
        response +=
            "reserved_memory_bytes:" + std::to_string(database_stats.reserved_memory_bytes) +
            "\r\n";
    }
    else
    {
        const auto database_stats = database.stats();
        response += "# Memory\r\n";
        response +=
            "used_memory_bytes:" + std::to_string(database_stats.approximate_memory_bytes) + "\r\n";
        response += "wal_bytes:" + std::to_string(database_stats.wal_bytes) + "\r\n";
        response +=
            "reserved_memory_bytes:" + std::to_string(database_stats.reserved_memory_bytes) +
            "\r\n";
    }

    append_bulk_view(output, response);
}

void execute_ping(const Command &, Database &, const ServerStats &, std::string &output)
{
    output.append("+PONG\r\n", 7);
}

void execute_set(const Command &command, Database &database, const ServerStats &,
                 std::string &output)
{
    const auto &arguments = command.arguments;
    if (arguments.size() != 2)
    {
        append_wrong_arguments(output, "set");
        return;
    }

    database.set(arguments[0], arguments[1]);
    require_reply_space(output, 5);
    output.append("+OK\r\n", 5);
}

void execute_get(const Command &command, Database &database, const ServerStats &,
                 std::string &output)
{
    const auto &arguments = command.arguments;
    if (arguments.size() != 1)
    {
        append_wrong_arguments(output, "get");
        return;
    }

    const auto value = database.value_view(arguments[0]);
    if (!value)
    {
        append_error(output, error_message(value.error().code()));
        return;
    }
    if (!*value)
    {
        append_null(output);
        return;
    }

    if ((*value)->kind == ValueView::Kind::integer)
    {
        append_bulk_string(output, std::to_string((*value)->integer));
        return;
    }
    if ((*value)->kind != ValueView::Kind::string)
    {
        append_error(output, error_message(ErrorCode::wrong_type));
        return;
    }
    append_bulk_view(output, (*value)->text);
}

void execute_increment(const Command &command, Database &database, const ServerStats &,
                       std::string &output)
{
    const auto &arguments = command.arguments;
    if (arguments.size() != 1)
    {
        append_wrong_arguments(output, "incr");
        return;
    }

    const auto value = database.increment(arguments[0]);
    if (!value)
    {
        append_error(output, error_message(value.error().code()));
        return;
    }
    append_integer(output, *value);
}

void execute_list_push(const Command &command, Database &database, const ServerStats &,
                       std::string &output, bool front)
{
    const auto &arguments = command.arguments;
    if (arguments.size() < 2)
    {
        append_wrong_arguments(output, command.name);
        return;
    }

    Result<std::int64_t> length = std::int64_t{0};
    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        length = front ? database.push_left_text(arguments[0], arguments[index])
                       : database.push_right_text(arguments[0], arguments[index]);
        if (!length) break;
    }

    if (!length)
    {
        append_error(output, error_message(length.error().code()));
        return;
    }
    append_integer(output, *length);
}

void execute_list_pop(const Command &command, Database &database, const ServerStats &,
                      std::string &output, bool front)
{
    const auto &arguments = command.arguments;
    if (arguments.size() != 1)
    {
        append_wrong_arguments(output, command.name);
        return;
    }

    const auto value = front ? database.pop_left(arguments[0]) : database.pop_right(arguments[0]);
    if (!value)
    {
        append_error(output, error_message(value.error().code()));
        return;
    }
    if (!*value)
    {
        append_null(output);
        return;
    }

    if (const auto *text = std::get_if<std::string>(&**value)) append_bulk_view(output, *text);
    else append_bulk_string(output, std::to_string(std::get<std::int64_t>(**value)));
}

void execute_list_length(const Command &command, Database &database, const ServerStats &,
                         std::string &output)
{
    const auto &arguments = command.arguments;
    if (arguments.size() != 1)
    {
        append_wrong_arguments(output, "llen");
        return;
    }

    const auto length = database.list_length(arguments[0]);
    if (!length)
    {
        append_error(output, error_message(length.error().code()));
        return;
    }
    append_integer(output, *length);
}

void execute_hash_set(const Command &command, Database &database, const ServerStats &,
                      std::string &output)
{
    const auto &arguments = command.arguments;
    if (arguments.size() < 3 || arguments.size() % 2 == 0)
    {
        append_wrong_arguments(output, "hset");
        return;
    }

    std::int64_t added = 0;
    for (std::size_t index = 1; index < arguments.size(); index += 2)
    {
        const auto result =
            database.hash_set_text(arguments[0], arguments[index], arguments[index + 1]);
        if (!result)
        {
            append_error(output, error_message(result.error().code()));
            return;
        }
        added += *result;
    }
    append_integer(output, added);
}

void execute_hash_get(const Command &command, Database &database, const ServerStats &,
                      std::string &output)
{
    const auto &arguments = command.arguments;
    if (arguments.size() != 2)
    {
        append_wrong_arguments(output, "hget");
        return;
    }

    const auto value = database.hash_field(arguments[0], arguments[1]);
    if (!value)
    {
        append_error(output, error_message(value.error().code()));
        return;
    }
    if (!*value)
    {
        append_null(output);
        return;
    }

    if ((*value)->kind == ValueView::Kind::integer)
    {
        append_bulk_string(output, std::to_string((*value)->integer));
        return;
    }
    append_bulk_view(output, (*value)->text);
}

void execute_hash_del(const Command &command, Database &database, const ServerStats &,
                      std::string &output)
{
    const auto &arguments = command.arguments;
    if (arguments.size() != 2)
    {
        append_wrong_arguments(output, "hdel");
        return;
    }

    const auto removed = database.hash_del(arguments[0], arguments[1]);
    if (!removed)
    {
        append_error(output, error_message(removed.error().code()));
        return;
    }
    append_integer(output, *removed ? 1 : 0);
}

void execute_hash_length(const Command &command, Database &database, const ServerStats &,
                         std::string &output)
{
    const auto &arguments = command.arguments;
    if (arguments.size() != 1)
    {
        append_wrong_arguments(output, "hlen");
        return;
    }

    const auto length = database.hash_length(arguments[0]);
    if (!length)
    {
        append_error(output, error_message(length.error().code()));
        return;
    }
    append_integer(output, *length);
}

void execute_expire(const Command &command, Database &database, const ServerStats &,
                    std::string &output)
{
    const auto &arguments = command.arguments;
    if (arguments.size() != 2)
    {
        append_wrong_arguments(output, "expire");
        return;
    }

    std::int64_t seconds = 0;
    const auto text = arguments[1];
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), seconds);
    if (error != std::errc{} || end != text.data() + text.size())
    {
        append_error(output, error_message(ErrorCode::value_not_integer));
        return;
    }
    try
    {
        append_integer(output, database.expire(arguments[0], seconds) ? 1 : 0);
    }
    catch (const Error &error)
    {
        if (error.code() != ErrorCode::value_not_integer) throw;
        append_error(output, error_message(error.code()));
    }
}

void execute_ttl(const Command &command, Database &database, const ServerStats &,
                 std::string &output)
{
    const auto &arguments = command.arguments;
    if (arguments.size() != 1)
    {
        append_wrong_arguments(output, "ttl");
        return;
    }
    append_integer(output, database.ttl(arguments[0]));
}

void execute_del(const Command &command, Database &database, const ServerStats &,
                 std::string &output)
{
    const auto &arguments = command.arguments;
    if (arguments.empty())
    {
        append_wrong_arguments(output, "del");
        return;
    }

    std::int64_t deleted = 0;
    for (const auto key : arguments)
        if (database.del(key)) ++deleted;
    append_integer(output, deleted);
}

}

void append_simple_string(std::string &output, std::string_view text)
{
    require_reply_space(output, text.size() + 3);
    output.push_back('+');
    output.append(text);
    output.append("\r\n", 2);
}

void append_error(std::string &output, std::string_view text)
{
    require_reply_space(output, text.size() + 3);
    output.push_back('-');
    output.append(text);
    output.append("\r\n", 2);
}

void append_integer(std::string &output, std::int64_t value)
{
    require_reply_space(output, 24);
    output.push_back(':');
    append_decimal(value, output);
    output.append("\r\n", 2);
}

void append_bulk_string(std::string &output, std::string_view text)
{
    const std::less<const char *> less;
    if (!less(text.data(), output.data()) && !less(output.data() + output.size(), text.data()))
    {
        const std::string saved(text);
        append_bulk_view(output, saved);
        return;
    }
    append_bulk_view(output, text);
}

void append_null(std::string &output)
{
    require_reply_space(output, 5);
    output.append("$-1\r\n", 5);
}

void append_array_header(std::string &output, std::int64_t count)
{
    require_reply_space(output, 24);
    output.push_back('*');
    append_decimal(count, output);
    output.append("\r\n", 2);
}

CommandInfo classify_command(std::string_view name) noexcept
{
    if (name.size() == 3)
    {
        const auto folded =
            (static_cast<std::uint32_t>(static_cast<unsigned char>(name[0])) | 0x20U) |
            ((static_cast<std::uint32_t>(static_cast<unsigned char>(name[1])) | 0x20U) << 8) |
            ((static_cast<std::uint32_t>(static_cast<unsigned char>(name[2])) | 0x20U) << 16);
        switch (folded)
        {
        case static_cast<std::uint32_t>(tag_of("get")):
            return {CommandId::get, false};
        case static_cast<std::uint32_t>(tag_of("set")):
            return {CommandId::set, true};
        case static_cast<std::uint32_t>(tag_of("del")):
            return {CommandId::del, true};
        case static_cast<std::uint32_t>(tag_of("ttl")):
            return {CommandId::ttl, false};
        default:
            return {CommandId::unknown, true};
        }
    }
    switch (tag_of(name))
    {
    case tag_of("ping"):
        return {CommandId::ping, false};
    case tag_of("info"):
        return {CommandId::info, false};
    case tag_of("get"):
        return {CommandId::get, false};
    case tag_of("set"):
        return {CommandId::set, true};
    case tag_of("incr"):
        return {CommandId::increment, true};
    case tag_of("lpush"):
        return {CommandId::list_push_left, true};
    case tag_of("rpush"):
        return {CommandId::list_push_right, true};
    case tag_of("lpop"):
        return {CommandId::list_pop_left, true};
    case tag_of("rpop"):
        return {CommandId::list_pop_right, true};
    case tag_of("llen"):
        return {CommandId::list_length, false};
    case tag_of("hset"):
        return {CommandId::hash_set, true};
    case tag_of("hget"):
        return {CommandId::hash_get, false};
    case tag_of("hdel"):
        return {CommandId::hash_del, true};
    case tag_of("hlen"):
        return {CommandId::hash_length, false};
    case tag_of("expire"):
        return {CommandId::expire, true};
    case tag_of("ttl"):
        return {CommandId::ttl, false};
    case tag_of("del"):
        return {CommandId::del, true};
    default:
        return {CommandId::unknown, true};
    }
}

void execute_into(const Command &command, Database &database, const ServerStats &stats,
                  std::string &output)
{
    execute_into(command, database, stats, output, classify_command(command.name).id);
}

void execute_into(const Command &command, Database &database, const ServerStats &stats,
                  std::string &output, CommandId classified_id)
{
    switch (classified_id)
    {
    case CommandId::ping:
        execute_ping(command, database, stats, output);
        return;
    case CommandId::info:
    {
        const auto &arguments = command.arguments;
        if (arguments.size() > 1)
        {
            append_wrong_arguments(output, "info");
            return;
        }
        const bool memory_only = !arguments.empty() && arguments[0] == "memory";
        if (!arguments.empty() && !memory_only)
        {
            append_error(output, error_message(ErrorCode::unsupported_info_section));
            return;
        }
        append_info(database, stats, memory_only, output);
        return;
    }
    case CommandId::get:
        execute_get(command, database, stats, output);
        return;
    case CommandId::set:
        execute_set(command, database, stats, output);
        return;
    case CommandId::increment:
        execute_increment(command, database, stats, output);
        return;
    case CommandId::list_push_left:
        execute_list_push(command, database, stats, output, true);
        return;
    case CommandId::list_push_right:
        execute_list_push(command, database, stats, output, false);
        return;
    case CommandId::list_pop_left:
        execute_list_pop(command, database, stats, output, true);
        return;
    case CommandId::list_pop_right:
        execute_list_pop(command, database, stats, output, false);
        return;
    case CommandId::list_length:
        execute_list_length(command, database, stats, output);
        return;
    case CommandId::hash_set:
        execute_hash_set(command, database, stats, output);
        return;
    case CommandId::hash_get:
        execute_hash_get(command, database, stats, output);
        return;
    case CommandId::hash_del:
        execute_hash_del(command, database, stats, output);
        return;
    case CommandId::hash_length:
        execute_hash_length(command, database, stats, output);
        return;
    case CommandId::expire:
        execute_expire(command, database, stats, output);
        return;
    case CommandId::ttl:
        execute_ttl(command, database, stats, output);
        return;
    case CommandId::del:
        execute_del(command, database, stats, output);
        return;
    case CommandId::unknown:
        break;
    }

    append_error(output, std::string(error_message(ErrorCode::unknown_command)) + " '" +
                             std::string{command.name} + "'");
}

namespace
{

template <typename T> Result<T> parse_number(std::string_view input, ErrorCode error_code)
{
    T value = 0;
    const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);
    if (error != std::errc{} || end != input.data() + input.size())
        return std::unexpected(Error{error_code});
    return value;
}

Result<void> parse_command_slow(std::string_view input, Command &command)
{
    command.arguments.clear();
    command.name = {};
    command.bytes_consumed = 0;

    if (input.empty()) return std::unexpected(Error{ErrorCode::empty_resp_input});
    if (input[0] != '*') return std::unexpected(Error{ErrorCode::expected_resp_array_prefix});

    std::size_t pos = 1;
    const auto readline = [&]() -> Result<std::string_view>
    {
        const std::size_t end = input.find("\r\n", pos);
        if (end == std::string_view::npos) return std::unexpected(Error{ErrorCode::missing_crlf});
        const auto line = input.substr(pos, end - pos);
        pos = end + 2;
        return line;
    };

    const auto argc_result = readline();
    if (!argc_result) return std::unexpected(argc_result.error());
    const auto argc =
        parse_number<std::size_t>(argc_result.value(), ErrorCode::invalid_resp_array_length);
    if (!argc) return std::unexpected(argc.error());
    if (*argc == 0) return std::unexpected(Error{ErrorCode::empty_resp_array});

    for (std::size_t index = 0; index < *argc; ++index)
    {
        if (pos >= input.size())
            return std::unexpected(Error{ErrorCode::missing_bulk_string_prefix});
        if (input[pos++] != '$')
            return std::unexpected(Error{ErrorCode::expected_bulk_string_prefix});

        const auto length_result = readline();
        if (!length_result) return std::unexpected(length_result.error());
        const auto length =
            parse_number<std::size_t>(length_result.value(), ErrorCode::invalid_bulk_string_length);
        if (!length) return std::unexpected(length.error());
        if (pos > input.size() || input.size() - pos < 2 || *length > input.size() - pos - 2)
            return std::unexpected(Error{ErrorCode::incomplete_bulk_string_data});

        const auto value = input.substr(pos, *length);
        if (index == 0) command.name = value;
        else command.arguments.push_back(value);

        pos += *length;
        if (input.size() - pos < 2)
            return std::unexpected(Error{ErrorCode::missing_bulk_string_terminator});
        if (input.compare(pos, 2, "\r\n") != 0)
            return std::unexpected(Error{ErrorCode::missing_bulk_string_terminator});
        pos += 2;
    }

    command.bytes_consumed = pos;
    return {};
}

inline bool read_length(const char *&cursor, const char *end, std::size_t &length) noexcept
{
    const auto remaining = end - cursor;
    if (remaining < 3) return false;
    const auto first = static_cast<unsigned char>(*cursor) - static_cast<unsigned>('0');
    if (first > 9) return false;
    if (cursor[1] == '\r' && cursor[2] == '\n')
    {
        length = first;
        cursor += 3;
        return true;
    }
    if (remaining < 4) return false;
    const auto second = static_cast<unsigned char>(cursor[1]) - static_cast<unsigned>('0');
    if (second > 9) return false;
    if (cursor[2] == '\r' && cursor[3] == '\n')
    {
        length = first * 10 + second;
        cursor += 4;
        return true;
    }
    const char *position = cursor;
    std::size_t value = 0;
    while (position != end)
    {
        const auto digit = static_cast<unsigned char>(*position) - static_cast<unsigned>('0');
        if (digit > 9) break;
        if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10) return false;
        value = value * 10 + digit;
        ++position;
    }
    if (end - position < 2 || position[0] != '\r' || position[1] != '\n') return false;
    length = value;
    cursor = position + 2;
    return true;
}
}

Result<void> parse_command_into(std::string_view input, Command &command)
{
    if (input.size() < 4 || input[0] != '*') return parse_command_slow(input, command);
    command.arguments.clear();
    command.name = {};
    command.bytes_consumed = 0;
    const char *cursor = input.data() + 1;
    const char *end = input.data() + input.size();
    std::size_t count;
    if (!read_length(cursor, end, count) || count == 0) return parse_command_slow(input, command);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (cursor == end || *cursor++ != '$') return parse_command_slow(input, command);
        std::size_t length;
        if (!read_length(cursor, end, length) || end - cursor < 2 ||
            length > static_cast<std::size_t>(end - cursor - 2))
            return parse_command_slow(input, command);
        if (cursor[length] != '\r' || cursor[length + 1] != '\n')
            return parse_command_slow(input, command);
        const std::string_view value(cursor, length);
        if (index == 0) command.name = value;
        else command.arguments.push_back(value);
        cursor += length + 2;
    }
    command.bytes_consumed = static_cast<std::size_t>(cursor - input.data());
    return {};
}

Result<Command> parse_command(std::string_view input)
{
    Command command;
    const auto parsed = parse_command_into(input, command);
    if (!parsed) return std::unexpected(parsed.error());
    return command;
}

}
