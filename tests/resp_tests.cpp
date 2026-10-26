#include <catch_amalgamated.hpp>

#include "resp.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace reddish;

TEST_CASE("RESP argument storage handles overflow reuse and oversized declared counts")
{
    Command parsed;
    std::string frame = "*41\r\n$3\r\nSET\r\n";
    for (int i = 0; i < 40; ++i)
        frame += "$1\r\na\r\n";
    REQUIRE(parse_command_into(frame, parsed));
    CHECK(parsed.arguments.size() == 40);
    for (const auto value : parsed.arguments)
        CHECK(value == "a");
    REQUIRE(parse_command_into("*2\r\n$3\r\nGET\r\n$1\r\nk\r\n", parsed));
    REQUIRE(parsed.arguments.size() == 1);
    CHECK(parsed.arguments[0] == "k");
    CHECK_FALSE(parse_command_into("*18446744073709551615\r\n", parsed));
    const std::string binary = std::string("*3\r\n$3\r\nSET\r\n$1\r\nk\r\n$3\r\na\0b\r\n", 29);
    for (std::size_t i = 0; i < binary.size(); ++i)
        CHECK_FALSE(parse_command_into(std::string_view(binary).substr(0, i), parsed));
    REQUIRE(parse_command_into(binary, parsed));
    CHECK(parsed.arguments[1] == std::string_view("a\0b", 3));
}

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

}

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

    REQUIRE(parse_command_into("*1\r\n$4\r\nPING\r\n", command));
    CHECK(command.name == "PING");
    CHECK(command.arguments.empty());

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

TEST_CASE("RESP parsing handles every frame prefix and decimal header boundary")
{
    Command parsed;
    for (const std::size_t length : {0, 9, 10, 32, 99, 100, 1000})
    {
        std::string value(length, 'x');
        if (length >= 4) value.replace(0, 4, std::string("\0\r\nx", 4));
        const std::string frame =
            "*3\r\n$3\r\nSET\r\n$1\r\nk\r\n$" + std::to_string(length) + "\r\n" + value + "\r\n";
        for (std::size_t prefix = 0; prefix < frame.size(); ++prefix)
            REQUIRE_FALSE(parse_command_into(std::string_view(frame).substr(0, prefix), parsed));
        const std::string pipeline = frame + "*1\r\n$4\r\nPING\r\n";
        REQUIRE(parse_command_into(pipeline, parsed));
        CHECK(parsed.name == "SET");
        REQUIRE(parsed.arguments.size() == 2);
        CHECK(parsed.arguments[1] == value);
        CHECK(parsed.bytes_consumed == frame.size());
    }
    REQUIRE(parse_command_into("*0002\r\n$003\r\nGET\r\n$0001\r\nk\r\n", parsed));
    CHECK(parsed.name == "GET");
    CHECK(parsed.arguments[0] == "k");
    check_parse_error("*999999999999999999999999999999\r\n", ErrorCode::invalid_resp_array_length);
    check_parse_error("*1\r\n$999999999999999999999999999999\r\n",
                      ErrorCode::invalid_bulk_string_length);
    check_parse_error("*1\r\n$12x\r\n", ErrorCode::invalid_bulk_string_length);
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

TEST_CASE("bulk encoding preserves prefixes and aliased binary payloads across growth")
{
    std::string output(1000, 'x');
    output[50] = '\0';
    const std::string original = output;
    const std::string payload = original.substr(40, 100);
    append_bulk_string(output, std::string_view(output).substr(40, 100));
    CHECK(output == original + "$100\r\n" + payload + "\r\n");
    const std::string saved = output;
    CHECK_THROWS_AS(append_bulk_string(output, std::string(1024 * 1024, 'y')), Error);
    CHECK(output == saved);
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
    for (const auto &[name, id] :
         {std::pair{"GET", CommandId::get}, std::pair{"SET", CommandId::set},
          std::pair{"DEL", CommandId::del}, std::pair{"TTL", CommandId::ttl}})
    {
        for (int mask = 0; mask < 8; ++mask)
        {
            std::string mixed = name;
            for (int index = 0; index < 3; ++index)
                if (mask & (1 << index)) mixed[index] += 'a' - 'A';
            CHECK(classify_command(mixed).id == id);
            CHECK(classify_command(mixed).is_write ==
                  (id == CommandId::set || id == CommandId::del));
        }
    }
    CHECK(classify_command("WHATEVER").id == CommandId::unknown);

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
