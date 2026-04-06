#include <catch_amalgamated.hpp>

#include "database.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace reddish;

namespace
{

std::filesystem::path temporary_dump_path(std::string_view name)
{
    return std::filesystem::temp_directory_path() /
           ("reddish-" + std::string(name) + "-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
            ".reddish");
}

void remove_dump(const std::filesystem::path &path)
{
    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(path.string() + ".tmp", error);
}

} // namespace

TEST_CASE("database stores strings and integers")
{
    Database database;

    database.set("name", "reddish");
    auto name = database.get("name");
    REQUIRE(name);
    REQUIRE(*name);
    CHECK(**name == "reddish");

    REQUIRE(database.increment("counter") == 1);
    REQUIRE(database.increment("counter") == 2);
    auto counter = database.get("counter");
    REQUIRE(counter);
    REQUIRE(*counter);
    CHECK(**counter == "2");

    database.set("number", "41");
    REQUIRE(database.increment("number") == 42);
    database.set("bad-number", "4x");
    const auto invalid = database.increment("bad-number");
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().code() == ErrorCode::value_not_integer);

    CHECK(database.del("name"));
    CHECK_FALSE(database.del("name"));
    auto missing = database.get("name");
    REQUIRE(missing);
    CHECK_FALSE(*missing);
}

TEST_CASE("database lists hold strings and integers")
{
    Database database;

    REQUIRE(database.push_right("queue", std::string{"last"}) == 1);
    REQUIRE(database.push_left("queue", std::int64_t{7}) == 2);
    REQUIRE(database.list_length("queue") == 2);

    auto first = database.pop_left("queue");
    REQUIRE(first);
    REQUIRE(*first);
    CHECK(std::get<std::int64_t>(**first) == 7);

    auto last = database.pop_right("queue");
    REQUIRE(last);
    REQUIRE(*last);
    CHECK(std::get<std::string>(**last) == "last");

    REQUIRE(database.list_length("queue") == 0);
    auto missing = database.pop_left("queue");
    REQUIRE(missing);
    CHECK_FALSE(*missing);

    database.set("string", "value");
    const auto wrong_type = database.push_left("string", std::string{"value"});
    REQUIRE_FALSE(wrong_type);
    CHECK(wrong_type.error().code() == ErrorCode::wrong_type);
}

TEST_CASE("database hashes store and delete values")
{
    Database database;

    REQUIRE(database.hash_set("user", "name", std::string{"Ada"}) == 1);
    REQUIRE(database.hash_set("user", "visits", std::int64_t{3}) == 1);
    REQUIRE(database.hash_set("user", "name", std::string{"Grace"}) == 0);
    REQUIRE(database.hash_length("user") == 2);

    auto name = database.hash_get("user", "name");
    REQUIRE(name);
    REQUIRE(*name);
    CHECK(std::get<std::string>(**name) == "Grace");

    CHECK(*database.hash_del("user", "name"));
    CHECK_FALSE(*database.hash_del("user", "missing"));
    CHECK(*database.hash_del("user", "visits"));
    REQUIRE(database.hash_length("user") == 0);

    database.set("string", "value");
    const auto wrong_type = database.hash_get("string", "field");
    REQUIRE_FALSE(wrong_type);
    CHECK(wrong_type.error().code() == ErrorCode::wrong_type);
}

TEST_CASE("database expires keys and evicts least recently used keys")
{
    Database database(2);
    database.set("first", "1");
    database.set("second", "2");
    REQUIRE(database.get("first"));
    database.set("third", "3");

    auto evicted = database.get("second");
    REQUIRE(evicted);
    CHECK_FALSE(*evicted);
    CHECK(database.stats().evictions == 1);

    CHECK(database.expire("first", 1));
    CHECK(database.ttl("first") >= 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    database.remove_expired();
    CHECK(database.ttl("first") == -2);
    CHECK(database.stats().expired_keys == 1);

    database.set("persistent", "yes");
    CHECK(database.ttl("persistent") == -1);
    CHECK(database.expire("persistent", 0));
    CHECK(database.ttl("persistent") == -2);
}

TEST_CASE("database snapshots round-trip and reject invalid input")
{
    const auto path = temporary_dump_path("snapshot");
    remove_dump(path);

    Database source;
    source.set("name", "reddish");
    REQUIRE(source.increment("counter") == 1);
    REQUIRE(source.push_right("items", std::string{"one"}) == 1);
    REQUIRE(source.push_right("items", std::int64_t{2}) == 2);
    REQUIRE(source.hash_set("meta", "version", std::int64_t{1}) == 1);
    REQUIRE(source.dump_to_disk(path));
    CHECK(std::filesystem::exists(path));
    CHECK(source.stats().snapshot_bytes > 0);
    CHECK(source.stats().last_dump_unix_ms > 0);

    Database restored;
    REQUIRE(restored.load_from_disk(path));
    auto name = restored.get("name");
    REQUIRE(name);
    REQUIRE(*name);
    CHECK(**name == "reddish");
    REQUIRE(restored.list_length("items") == 2);
    auto count = restored.hash_get("meta", "version");
    REQUIRE(count);
    REQUIRE(*count);
    CHECK(std::get<std::int64_t>(**count) == 1);

    Database too_small(1);
    const auto capacity_error = too_small.load_from_disk(path);
    REQUIRE_FALSE(capacity_error);
    CHECK(capacity_error.error().code() == ErrorCode::dump_exceeds_key_capacity);

    const auto corrupt_path = temporary_dump_path("corrupt");
    {
        std::ofstream corrupt(corrupt_path, std::ios::binary);
        corrupt << "not a snapshot";
    }
    const auto corrupt = restored.load_from_disk(corrupt_path);
    REQUIRE_FALSE(corrupt);
    CHECK(corrupt.error().code() == ErrorCode::disk_read_failed);

    const auto absent = restored.load_from_disk(temporary_dump_path("missing"));
    REQUIRE_FALSE(absent);
    CHECK(absent.error().code() == ErrorCode::disk_read_failed);

    remove_dump(path);
    remove_dump(corrupt_path);
}

TEST_CASE("database snapshots omit keys that expire before the dump")
{
    const auto path = temporary_dump_path("expired");
    remove_dump(path);

    Database source;
    source.set("temporary", "value");
    REQUIRE(source.expire("temporary", 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    REQUIRE(source.dump_to_disk(path));

    Database restored;
    REQUIRE(restored.load_from_disk(path));
    const auto value = restored.get("temporary");
    REQUIRE(value);
    CHECK_FALSE(*value);

    remove_dump(path);
}
