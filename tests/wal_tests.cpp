#include "database.hpp"
#include <catch_amalgamated.hpp>
#include <filesystem>
#include <fstream>
#include <thread>
using namespace reddish;
namespace
{
struct LogFile
{
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("reddish-wal-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".wal");
    ~LogFile()
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};
} // namespace
TEST_CASE("WAL replays every mutation including expiry and LRU eviction")
{
    LogFile log;
    {
        Database db(3);
        db.open_wal(log.path, WalSync::always);
        db.set("gone", "v");
        REQUIRE(db.del("gone"));
        REQUIRE(db.increment("counter") == 1);
        REQUIRE(db.increment("counter") == 2);
        REQUIRE(db.push_left("list", std::string("left")));
        REQUIRE(db.push_right("list", std::int64_t(7)));
        REQUIRE(db.pop_left("list"));
        REQUIRE(db.hash_set("hash", "n", std::int64_t(42)));
        REQUIRE(db.hash_set_text("hash", "large", std::string(70000, 'x')));
        REQUIRE(db.hash_del("hash", "n") == true);
        REQUIRE(db.expire("list", 30));
        db.set("victim", "v");
        REQUIRE(db.evict_to(3, 100) == 1);
        db.flush_wal();
        CHECK(db.stats().wal_bytes > 8);
    }
    Database restored(3);
    restored.open_wal(log.path);
    CHECK(restored.key_count() == 3);
    CHECK(restored.ttl("list") >= 0);
    CHECK(restored.list_length("list") == 1);
    auto item = restored.pop_right("list");
    REQUIRE(item);
    REQUIRE(*item);
    CHECK(std::get<std::int64_t>(**item) == 7);
    auto large = restored.hash_get("hash", "large");
    REQUIRE(large);
    REQUIRE(*large);
    CHECK(std::get<std::string>(**large).size() == 70000);
    CHECK_FALSE(*restored.get("counter")); // oldest at the explicit eviction
    restored.flush_wal();
}
TEST_CASE("WAL TTLs survive restart without being extended")
{
    LogFile log;
    {
        Database db;
        db.open_wal(log.path);
        db.set("ttl", "v");
        REQUIRE(db.expire("ttl", 1));
        db.flush_wal();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    {
        Database db;
        db.open_wal(log.path);
        CHECK_FALSE(*db.get("ttl"));
        db.flush_wal();
    }
    {
        Database db;
        db.open_wal(log.path);
        CHECK_FALSE(*db.get("ttl"));
    }
}
TEST_CASE("WAL repairs an incomplete final frame but rejects corruption")
{
    LogFile log;
    {
        Database db;
        db.open_wal(log.path);
        db.set("k", "v");
        db.flush_wal();
    }
    const auto valid = std::filesystem::file_size(log.path);
    {
        std::ofstream out(log.path, std::ios::binary | std::ios::app);
        out.write("\x10\0\0", 3);
    }
    {
        Database db;
        db.open_wal(log.path);
        CHECK(**db.get("k") == "v");
        CHECK(std::filesystem::file_size(log.path) == valid);
        db.set("new", "value");
        db.flush_wal();
    }
    {
        std::fstream out(log.path, std::ios::binary | std::ios::in | std::ios::out);
        out.seekp(16);
        out.put('\x7f');
    }
    Database db;
    CHECK_THROWS_AS(db.open_wal(log.path), Error);
}
TEST_CASE("WAL invalid headers and unwritable paths fail explicitly")
{
    LogFile log;
    {
        std::ofstream out(log.path);
        out << "snapshot";
    }
    Database db;
    CHECK_THROWS_AS(db.open_wal(log.path), Error);
    Database other;
    CHECK_THROWS_AS(other.open_wal(log.path / "missing.wal"), std::exception);
}
TEST_CASE("a WAL has one writer at a time")
{
    LogFile log;
    Database first;
    first.open_wal(log.path);
    Database second;
    CHECK_THROWS_AS(second.open_wal(log.path), Error);
    first.set("k", "v");
    first.flush_wal();
}

TEST_CASE("failed WAL replay clears partial state and permits a clean retry")
{
    LogFile log;
    LogFile clean;
    std::uintmax_t valid = 0;
    {
        Database db;
        db.open_wal(log.path, WalSync::always);
        db.set("string", "value");
        REQUIRE(db.hash_set_text("hash", "field", std::string(70000, 'h')));
        REQUIRE(db.expire("hash", 60));
        valid = std::filesystem::file_size(log.path);
        db.set("corrupt", "record");
    }
    {
        std::fstream out(log.path, std::ios::binary | std::ios::in | std::ios::out);
        out.seekp(static_cast<std::streamoff>(valid + 8));
        out.put('\x7f');
        REQUIRE(out.good());
    }
    Database db;
    REQUIRE_THROWS_AS(db.open_wal(log.path), Error);
    CHECK(db.key_count() == 0);
    CHECK_FALSE(db.maintenance_needed());
    CHECK_FALSE(*db.get("string"));
    CHECK_FALSE(*db.hash_get("hash", "field"));
    CHECK(db.stats().wal_bytes == 0);
    REQUIRE_NOTHROW(db.open_wal(clean.path, WalSync::always));
    db.set("retry", "works");
    db.flush_wal();
    CHECK(**db.get("retry") == "works");
}
