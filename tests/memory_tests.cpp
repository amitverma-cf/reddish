#include "database.hpp"
#include <array>
#include <catch_amalgamated.hpp>
#include <filesystem>
using namespace reddish;

TEST_CASE("owned memory accounting includes slabs metadata and direct allocations")
{
    MemoryResource resource(200000);
    {
        SlabPool pool(&resource);
        auto *small = pool.allocate(72);
        CHECK(resource.used() > pool.reserved_bytes());
        auto *large = pool.allocate(8192);
        const auto before = resource.used();
        pool.deallocate(large, 8192);
        CHECK(resource.used() == before - 8192);
        pool.deallocate(small, 72);
        pool.reclaim();
        CHECK(resource.used() == 0);
    }
    CHECK(resource.used() == 0);
}

TEST_CASE("database allocation limits preserve rejected values and TTLs")
{
    Database db(10000, 300000);
    db.set("target", "original");
    REQUIRE(db.expire("target", 60));
    REQUIRE_THROWS_AS(db.set("target", std::string(400000, 'x')), MemoryLimit);
    CHECK(**db.get("target") == "original");
    CHECK(db.ttl("target") >= 0);
    CHECK(db.stats().owned_memory_bytes <= db.stats().max_memory_bytes);
    REQUIRE_THROWS_AS(Database(10, 64), MemoryLimit);
}

TEST_CASE("memory pressure reclaims empty slabs across value sizes")
{
    Database db(10000, 300000);
    for (int i = 0; i < 2000; ++i)
        db.set(std::to_string(i), std::string(32, 'x'));
    for (int i = 0; i < 2000; ++i)
        REQUIRE(db.del(std::to_string(i)));
    const auto retained = db.stats().owned_memory_bytes;
    REQUIRE(retained > 100000);
    db.set("large", std::string(230000, 'v'));
    CHECK(db.stats().owned_memory_bytes <= 300000);
    CHECK((**db.get("large")).size() == 230000);
    REQUIRE(db.del("large"));
    CHECK(db.stats().owned_memory_bytes < retained);
}

TEST_CASE("multi value writes reserve memory before changing collections")
{
    Database db(10000, 300000);
    REQUIRE(db.push_right_text("list", "original") == 1);
    REQUIRE(db.hash_set_text("hash", "old", "value") == 1);
    REQUIRE(db.expire("list", 60));
    REQUIRE(db.expire("hash", 60));
    const std::string large(400000, 'x');
    const std::array<std::string_view, 2> values{"small", large};
    REQUIRE_THROWS_AS(db.push_texts("list", values, false), MemoryLimit);
    CHECK(db.list_length("list") == 1);
    const std::array<std::string_view, 4> fields{"new", "small", "large", large};
    REQUIRE_THROWS_AS(db.hash_set_texts("hash", fields), MemoryLimit);
    CHECK(db.hash_length("hash") == 1);
    CHECK(!*db.hash_field("hash", "new"));
    CHECK(db.ttl("hash") >= 0);
    CHECK(db.ttl("list") >= 0);
    CHECK(db.stats().owned_memory_bytes <= 300000);
}

TEST_CASE("quota failures leave WAL replay equal to live collections")
{
    auto path =
        std::filesystem::temp_directory_path() /
        ("reddish-quota-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".wal");
    {
        Database db(10000, 300000);
        db.open_wal(path, WalSync::always);
        REQUIRE(db.push_right_text("list", "original") == 1);
        REQUIRE(db.hash_set_text("hash", "old", "value") == 1);
        REQUIRE(db.expire("hash", 60));
        const auto bytes = db.stats().wal_bytes;
        REQUIRE_THROWS_AS(db.push_right_text("list", std::string(400000, 'x')), MemoryLimit);
        REQUIRE_THROWS_AS(db.hash_set_text("hash", "new", std::string(400000, 'x')), MemoryLimit);
        CHECK(db.stats().wal_bytes == bytes);
    }
    {
        Database db(10000, 300000);
        db.open_wal(path);
        CHECK(db.list_length("list") == 1);
        CHECK(db.hash_length("hash") == 1);
        CHECK(db.ttl("hash") >= 0);
    }
    std::filesystem::remove(path);
    path += ".lock";
    std::filesystem::remove(path);
}
