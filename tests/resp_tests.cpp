#include <catch_amalgamated.hpp>

#include "resp.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace reddish;

namespace
{

Command command(std::string_view name, std::initializer_list<std::string_view> arguments = {})
{
    Command parsed;
    parsed.name = name;
    for (const auto value : arguments)
        parsed.arguments.push_back(value);
    return parsed;
}

ServerStats stats_for()
{
    return {3, std::chrono::steady_clock::now() - std::chrono::seconds(12), 4, 5};
}

std::string run(Database &database, Command input)
{
    std::string output;
    execute_into(input, database, stats_for(), output);
    return output;
}

void check_parse_error(std::string_view input, ErrorCode code)
{
    const auto result = parse_command(input);
    REQUIRE_FALSE(result);
    CHECK(result.error().code() == code);
}

} // namespace

TEST_CASE("RESP parser reads a command and leaves pipelined bytes")
{
    constexpr std::string_view input = "*2\r\n$3\r\nGET\r\n$4\r\nname\r\n*1\r\n$4\r\nPING\r\n";
    const auto parsed = parse_command(input);

    REQUIRE(parsed);
    CHECK(parsed->name == "GET");
    REQUIRE(parsed->arguments.size() == 1);
    CHECK(parsed->arguments[0] == "name");
    CHECK(parsed->bytes_consumed == 23);
    CHECK(input.substr(parsed->bytes_consumed) == "*1\r\n$4\r\nPING\r\n");
}

TEST_CASE("RESP parser reuses an existing command slot without reallocating")
{
    Command command;
    REQUIRE(parse_command_into("*3\r\n$3\r\nSET\r\n$1\r\na\r\n$1\r\nb\r\n", command));
    CHECK(command.arguments.size() == 2);

    // Reusing the slot must drop the previous arguments rather than append.
    REQUIRE(parse_command_into("*1\r\n$4\r\nPING\r\n", command));
    CHECK(command.name == "PING");
    CHECK(command.arguments.empty());

    // More arguments than the inline capacity must still work.
    std::string wide = "*10\r\n$5\r\nLPUSH\r\n$1\r\nk\r\n";
    for (int index = 0; index < 8; ++index)
        wide += "$1\r\n" + std::to_string(index) + "\r\n";
    REQUIRE(parse_command_into(wide, command));
    CHECK(command.name == "LPUSH");
    REQUIRE(command.arguments.size() == 9);
    CHECK(command.arguments[0] == "k");
    CHECK(command.arguments[8] == "7");
}

TEST_CASE("RESP parser rejects malformed input")
{
    check_parse_error("", ErrorCode::empty_resp_input);
    check_parse_error("+OK\r\n", ErrorCode::expected_resp_array_prefix);
    check_parse_error("*", ErrorCode::missing_crlf);
    check_parse_error("*x\r\n", ErrorCode::invalid_resp_array_length);
    check_parse_error("*0\r\n", ErrorCode::empty_resp_array);
    check_parse_error("*1\r\n", ErrorCode::missing_bulk_string_prefix);
    check_parse_error("*1\r\n+3\r\n", ErrorCode::expected_bulk_string_prefix);
    check_parse_error("*1\r\n$x\r\n", ErrorCode::invalid_bulk_string_length);
    check_parse_error("*1\r\n$4\r\nPI", ErrorCode::incomplete_bulk_string_data);
    check_parse_error("*1\r\n$4\r\nPING", ErrorCode::incomplete_bulk_string_data);
    check_parse_error("*1\r\n$4\r\nPINGxx", ErrorCode::missing_bulk_string_terminator);
}

TEST_CASE("RESP encoder emits every response type")
{
    std::string output;

    append_simple_string(output, "OK");
    CHECK(output == "+OK\r\n");

    output.clear();
    append_error(output, "ERR failed");
    CHECK(output == "-ERR failed\r\n");

    output.clear();
    append_integer(output, -12);
    CHECK(output == ":-12\r\n");

    output.clear();
    append_bulk_string(output, "hello");
    CHECK(output == "$5\r\nhello\r\n");

    output.clear();
    append_null(output);
    CHECK(output == "$-1\r\n");

    output.clear();
    append_array_header(output, 2);
    append_integer(output, 1);
    append_bulk_string(output, "x");
    CHECK(output == "*2\r\n:1\r\n$1\r\nx\r\n");
}

TEST_CASE("command dispatch ignores name casing and rejects unknown commands")
{
    CHECK(classify_command("GET").id == CommandId::get);
    CHECK(classify_command("get").id == CommandId::get);
    CHECK(classify_command("SeT").id == CommandId::set);
    CHECK(classify_command("PING").is_write == false);
    CHECK(classify_command("SET").is_write == true);
    CHECK(classify_command("DEL").is_write == true);
    CHECK(classify_command("GET").is_write == false);
    CHECK(classify_command("WHATEVER").id == CommandId::unknown);
    // A name with an embedded NUL must not alias the shorter command.
    CHECK(classify_command(std::string_view{"get\0\0\0\0\0", 8}).id == CommandId::unknown);

    Database database;
    CHECK(run(database, command("get", {"missing"})) == "$-1\r\n");
}

TEST_CASE("command execution handles strings integers and deletion")
{
    Database database;

    CHECK(run(database, command("ping")) == "+PONG\r\n");
    CHECK(run(database, command("SET", {"name", "reddish"})) == "+OK\r\n");
    CHECK(run(database, command("GET", {"name"})) == "$7\r\nreddish\r\n");
    CHECK(run(database, command("INCR", {"counter"})) == ":1\r\n");
    CHECK(run(database, command("DEL", {"name", "counter", "missing"})) == ":2\r\n");
    CHECK(run(database, command("GET", {"name"})) == "$-1\r\n");
    CHECK(run(database, command("DEL")) == "-ERR wrong number of arguments for 'del' command\r\n");
}

TEST_CASE("command execution handles lists and hashes")
{
    Database database;

    CHECK(run(database, command("LPUSH", {"queue", "last", "first"})) == ":2\r\n");
    CHECK(run(database, command("RPOP", {"queue"})) == "$4\r\nlast\r\n");
    CHECK(run(database, command("LPOP", {"queue"})) == "$5\r\nfirst\r\n");
    CHECK(run(database, command("LLEN", {"queue"})) == ":0\r\n");
    CHECK(run(database, command("LPUSH", {"queue"})) ==
          "-ERR wrong number of arguments for 'LPUSH' command\r\n");

    CHECK(run(database, command("HSET", {"user", "name", "Ada", "role", "dev"})) == ":2\r\n");
    CHECK(run(database, command("HGET", {"user", "name"})) == "$3\r\nAda\r\n");
    CHECK(run(database, command("HLEN", {"user"})) == ":2\r\n");
    CHECK(run(database, command("HDEL", {"user", "name"})) == ":1\r\n");
    CHECK(run(database, command("HSET", {"user", "name"})) ==
          "-ERR wrong number of arguments for 'hset' command\r\n");

    // A hash field stored as an integer still answers with a bulk string, exactly
    // like Redis does, rather than leaking the internal integer encoding.
    CHECK(run(database, command("HSET", {"counters", "hits", "7"})) == ":1\r\n");
    CHECK(run(database, command("HGET", {"counters", "hits"})) == "$1\r\n7\r\n");
}

TEST_CASE("command execution handles expiry and INFO")
{
    Database database;
    database.set("key", "value");

    CHECK(run(database, command("EXPIRE", {"key", "10"})) == ":1\r\n");
    CHECK(run(database, command("TTL", {"key"})).starts_with(":"));
    CHECK(run(database, command("EXPIRE", {"key", "invalid"})) ==
          "-ERR value is not an integer or out of range\r\n");
    CHECK(run(database, command("EXPIRE", {"key", "9223372036854775807"})) ==
          "-ERR value is not an integer or out of range\r\n");
    CHECK(run(database, command("INFO", {"memory"})).find("# Memory\r\n") != std::string::npos);
    CHECK(run(database, command("INFO")).find("connected_clients:3\r\n") != std::string::npos);
    CHECK(run(database, command("INFO", {"clients"})) == "-ERR unsupported INFO section\r\n");
    CHECK(run(database, command("INFO", {"memory", "extra"})) ==
          "-ERR wrong number of arguments for 'info' command\r\n");
}

TEST_CASE("command execution reports wrong arguments types and unknown commands")
{
    Database database;
    database.set("string", "value");

    CHECK(run(database, command("SET", {"one"})) ==
          "-ERR wrong number of arguments for 'set' command\r\n");
    CHECK(run(database, command("GET", {"one", "two"})) ==
          "-ERR wrong number of arguments for 'get' command\r\n");
    CHECK(run(database, command("INCR", {"one", "two"})) ==
          "-ERR wrong number of arguments for 'incr' command\r\n");
    CHECK(run(database, command("LPOP", {"string"})) ==
          "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n");
    CHECK(run(database, command("EXPIRE", {"key"})) ==
          "-ERR wrong number of arguments for 'expire' command\r\n");
    CHECK(run(database, command("TTL", {"key", "extra"})) ==
          "-ERR wrong number of arguments for 'ttl' command\r\n");
    CHECK(run(database, command("WHATEVER")) == "-ERR unknown command 'WHATEVER'\r\n");
}
