#pragma once
#include "result.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace reddish
{

struct StringHash
{
    using is_transparent = void;
    std::size_t operator()(std::string_view value) const
    {
        return std::hash<std::string_view>{}(value);
    }
    std::size_t operator()(const std::string &value) const
    {
        return (*this)(std::string_view{value});
    }
};

struct StringEqual
{
    using is_transparent = void;
    bool operator()(std::string_view left, std::string_view right) const
    {
        return left == right;
    }
};

struct List;
struct Hash;
using ListPtr = std::shared_ptr<List>;
using HashPtr = std::shared_ptr<Hash>;
using Value = std::variant<std::string, std::int64_t, ListPtr, HashPtr>;

struct List
{
    std::deque<Value> values;
};

struct Hash
{
    std::unordered_map<std::string, Value, StringHash, StringEqual> fields;
};

struct DatabaseStats
{
    std::size_t key_count;
    std::size_t approximate_memory_bytes;
    std::size_t snapshot_bytes;
    std::uint64_t evictions;
    std::uint64_t expired_keys;
    std::int64_t last_dump_unix_ms;
};

class Database
{
  private:
    using KeyOrder = std::list<std::string_view>;

    struct Entry
    {
        Value value;
        KeyOrder::iterator lru_position;
        std::optional<std::chrono::steady_clock::time_point> expires_at;
    };

    struct Expiry
    {
        std::chrono::steady_clock::time_point deadline;
        std::string key;
    };

    struct ExpiryLater
    {
        bool operator()(const Expiry &left, const Expiry &right) const
        {
            return left.deadline > right.deadline;
        }
    };

    using Store = std::unordered_map<std::string, Entry, StringHash, StringEqual>;
    using ExpiryQueue = std::priority_queue<Expiry, std::vector<Expiry>, ExpiryLater>;

    Store kv_store;
    KeyOrder lru_keys;
    ExpiryQueue expirations;
    std::size_t max_keys;
    std::size_t ttl_key_count = 0;
    std::uint64_t evictions = 0;
    std::uint64_t expired_keys = 0;
    std::size_t snapshot_bytes = 0;
    std::int64_t last_dump_unix_ms = -1;

  public:
    explicit Database(std::size_t max_keys = 1024);

    void set(std::string_view key, std::string_view val);

    Result<std::optional<std::string>> get(std::string_view key);
    Result<std::optional<std::reference_wrapper<const Value>>> get_value(std::string_view key);

    Result<std::int64_t> increment(std::string_view key);

    Result<std::int64_t> push_left(std::string_view key, Value value);
    Result<std::int64_t> push_right(std::string_view key, Value value);
    Result<std::optional<Value>> pop_left(std::string_view key);
    Result<std::optional<Value>> pop_right(std::string_view key);
    Result<std::int64_t> list_length(std::string_view key);

    Result<std::int64_t> hash_set(std::string_view key, std::string_view field, Value value);
    Result<std::optional<Value>> hash_get(std::string_view key, std::string_view field);
    Result<bool> hash_del(std::string_view key, std::string_view field);
    Result<std::int64_t> hash_length(std::string_view key);

    bool expire(std::string_view key, std::int64_t seconds);
    std::int64_t ttl(std::string_view key);
    void remove_expired();
    Result<void> dump_to_disk(const std::filesystem::path &path);
    Result<void> load_from_disk(const std::filesystem::path &path);
    DatabaseStats stats() const;

    bool del(std::string_view key);

  private:
    Result<ListPtr> get_or_create_list(std::string_view key);
    Result<ListPtr> find_list(std::string_view key);
    Result<HashPtr> get_or_create_hash(std::string_view key);
    Result<HashPtr> find_hash(std::string_view key);
    Entry &insert(std::string_view key, Value value);
    Store::iterator find_active(std::string_view key);
    void erase(Store::iterator entry);
    void touch(Store::iterator entry);
    void evict_if_full();
    void schedule_expiry(std::string_view key, std::chrono::steady_clock::time_point deadline);
    void clear_expiry(Entry &entry);
};

} // namespace reddish
