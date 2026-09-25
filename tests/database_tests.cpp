#include <catch_amalgamated.hpp>

#include "database.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace reddish;

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
    REQUIRE(database.evict_to(2, 128) == 1);

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

TEST_CASE("LRU survives keyspace rehashing")
{
    // Enough keys to force several rehashes of the bucket array.
    Database database(257);
    for (int index = 0; index < 257; ++index)
        database.set("key-" + std::to_string(index), "value");

    // Touch the oldest key, then evict exactly the least recent remaining key.
    REQUIRE(database.get("key-0"));
    database.set("overflow", "value");
    REQUIRE(database.evict_to(257, 128) == 1);

    CHECK(database.stats().evictions == 1);
    CHECK(database.stats().key_count == 257);
    CHECK_FALSE(*database.get("key-1"));

    const auto most_recent = database.get("key-0");
    REQUIRE(most_recent);
    REQUIRE(*most_recent);
    CHECK(**most_recent == "value");
}

TEST_CASE("small keyspaces evict exactly the least recently used key")
{
    Database database(4);
    database.set("a", "1");
    database.set("b", "2");
    database.set("c", "3");
    database.set("d", "4");

    REQUIRE(database.get("a"));
    REQUIRE(database.get("b"));
    database.set("e", "5");
    REQUIRE(database.evict_to(4, 128) == 1);

    auto oldest = database.get("c");
    REQUIRE(oldest);
    CHECK_FALSE(*oldest);
    CHECK(database.stats().evictions == 1);

    for (const auto key : {"a", "b", "d", "e"})
    {
        auto kept = database.get(key);
        REQUIRE(kept);
        CHECK(*kept);
    }
}

TEST_CASE("database ignores stale expiry records")
{
    Database database;
    database.set("key", "value");
    REQUIRE(database.expire("key", 1));
    REQUIRE(database.expire("key", 2));

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    database.remove_expired();

    const auto value = database.get("key");
    REQUIRE(value);
    REQUIRE(*value);
    CHECK(**value == "value");

    database.set("persistent", "value");
    REQUIRE(database.expire("persistent", 1));
    database.set("persistent", "new-value");

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    database.remove_expired();

    const auto persistent = database.get("persistent");
    REQUIRE(persistent);
    REQUIRE(*persistent);
    CHECK(**persistent == "new-value");
}

TEST_CASE("large hash inserts and replacements promote safely")
{
    for (bool replacement : {false, true})
    {
        Database db;
        if (replacement) REQUIRE(db.hash_set_text("h", "f", "small"));
        const std::string value(70000, 'x');
        REQUIRE(db.hash_set_text("h", "f", value));
        REQUIRE(db.hash_set_text("h", "neighbor", "untouched"));
        auto got = db.hash_get("h", "f");
        REQUIRE(got);
        REQUIRE(*got);
        CHECK(std::get<std::string>(**got) == value);
        REQUIRE(db.hash_del("h", "f") == true);
        CHECK(db.hash_length("h") == 1);
        const std::string key(4100, 'k');
        for (int i = 0; i < 130; ++i)
            REQUIRE(db.hash_set_text(key, std::to_string(i), "v"));
        REQUIRE(db.hash_get(key, "1"));
    }
}
TEST_CASE("constant-length queues reuse packed capacity")
{
    for (bool left : {false, true})
    {
        Database db;
        REQUIRE(db.push_right_text("q", "v"));
        for (int i = 0; i < 100000; ++i)
        {
            if (left)
            {
                REQUIRE(db.push_left_text("q", "v"));
                REQUIRE(db.pop_right("q"));
            }
            else
            {
                REQUIRE(db.push_right_text("q", "v"));
                REQUIRE(db.pop_left("q"));
            }
        }
        CHECK(db.list_length("q") == 1);
        CHECK(db.stats().approximate_memory_bytes < 1024);
    }
}
TEST_CASE("expiry rescheduling keeps one heap entry and follows grown nodes")
{
    Database db;
    db.set("k", "v");
    for (int i = 0; i < 10000; ++i)
        REQUIRE(db.expire("k", 10));
    CHECK(db.stats().approximate_memory_bytes < 2048);
    db.set("other", "x");
    REQUIRE(db.expire("other", 1));
    REQUIRE(db.push_right_text("list", "x"));
    REQUIRE(db.expire("list", 1));
    REQUIRE(db.push_right_text("list", std::string(10000, 'x')));
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    CHECK(db.list_length("list") == 0);
    // A read observes expiry without changing the keyspace.
    CHECK(db.key_count() == 3);
    db.remove_expired(1);
    CHECK(db.key_count() == 2);
    db.remove_expired(1);
    CHECK(db.key_count() == 1);
    CHECK(db.ttl("k") >= 0);
}
TEST_CASE("large overwritten values and drained lists release excess storage")
{
    Database db;
    db.set("k", std::string(70000, 'x'));
    auto view = db.value_view("k");
    REQUIRE(view);
    REQUIRE(*view);
    db.set("k", (*view)->text.substr(0, 1));
    REQUIRE(db.get("k"));
    CHECK(**db.get("k") == "x");
    REQUIRE(db.push_right_text("q", std::string(70000, 'x')));
    REQUIRE(db.push_right_text("q", "remaining"));
    REQUIRE(db.expire("q", 20));
    REQUIRE(db.pop_left("q"));
    CHECK(db.list_length("q") == 1);
    CHECK(db.ttl("q") >= 0);
    CHECK(db.stats().approximate_memory_bytes < 4096);
    auto remaining = db.pop_right("q");
    REQUIRE(remaining);
    REQUIRE(*remaining);
    CHECK(std::get<std::string>(**remaining) == "remaining");
}
