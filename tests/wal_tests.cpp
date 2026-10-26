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
        auto lock = path;
        lock += ".lock";
        std::filesystem::remove(lock, ec);
    }
};
}
TEST_CASE("WAL compaction preserves types collections deadlines and writer exclusion")
{
    LogFile log;
    std::size_t before;
    {
        Database db(10000);
        db.open_wal(log.path, WalSync::always);
        db.begin_wal_batch();
        for (int i = 0; i < 1000; ++i)
            db.set("value", std::to_string(i));
        REQUIRE(db.increment("integer") == 1);
        REQUIRE(db.push_right("list", std::int64_t(7)) == 1);
        REQUIRE(db.push_right_text("list", std::string_view("a\0b", 3)) == 2);
        REQUIRE(db.hash_set("hash", "n", std::int64_t(42)) == 1);
        REQUIRE(db.hash_set_text("promoted", "large", std::string(70000, 'x')) == 1);
        REQUIRE(db.expire("list", 30));
        db.set("gone", "v");
        REQUIRE(db.expire("gone", 0));
        db.end_wal_batch();
        before = db.stats().wal_bytes;
        db.compact_wal();
        CHECK(db.stats().wal_bytes < before);
        Database other;
        REQUIRE_THROWS(other.open_wal(log.path));
        db.set("after", "replacement");
    }
    {
        Database db;
        db.open_wal(log.path);
        CHECK(**db.get("value") == "999");
        CHECK(db.increment("integer") == 2);
        CHECK(std::get<std::int64_t>(**db.pop_left("list")) == 7);
        CHECK(std::get<std::string>(**db.pop_left("list")) == std::string("a\0b", 3));
        CHECK(std::get<std::int64_t>(**db.hash_get("hash", "n")) == 42);
        CHECK(std::get<std::string>(**db.hash_get("promoted", "large")) == std::string(70000, 'x'));
        CHECK(!*db.get("gone"));
        CHECK(**db.get("after") == "replacement");
    }
}

TEST_CASE("failed compaction serialization leaves the original WAL writable")
{
    LogFile log;
    {
        Wal wal(log.path, WalSync::always, [](WalOp, const auto &) {});
        wal.append(WalOp::set, {"key", "old"});
        REQUIRE_THROWS(wal.compact(
            [](Wal &replacement)
            {
                replacement.append(WalOp::set, {"key", "incomplete"});
                throw std::bad_alloc{};
            }));
        CHECK_FALSE(wal.failed());
        wal.append(WalOp::set, {"key", "after"});
    }
    Database db;
    db.open_wal(log.path);
    CHECK(**db.get("key") == "after");
}

TEST_CASE("compaction preserves expiry deadlines and LRU order")
{
    LogFile log;
    {
        Database db;
        db.open_wal(log.path);
        db.set("old", "a");
        db.set("keep", "b");
        db.set("expires", "c");
        REQUIRE(db.expire("expires", 1));
        REQUIRE(db.get("old"));
        db.compact_wal();
    }
    {
        Database db;
        db.open_wal(log.path);
        REQUIRE(db.evict_to(2, 1) == 1);
        CHECK(!*db.get("keep"));
        CHECK(**db.get("old") == "a");
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        CHECK(!*db.get("expires"));
        db.compact_wal();
    }
    Database db;
    db.open_wal(log.path);
    CHECK(!*db.get("expires"));
    CHECK(**db.get("old") == "a");
}
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
    CHECK_FALSE(*restored.get("counter"));
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
