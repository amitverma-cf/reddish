#pragma once
#include "result.hpp"

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace reddish
{

struct List;
using ListPtr = std::shared_ptr<List>;
using Value = std::variant<std::string, std::int64_t, ListPtr>;

struct List
{
    std::vector<Value> values;
};

class Database
{
  private:
    using KeyOrder = std::list<std::string>;

    struct Entry
    {
        Value value;
        KeyOrder::iterator lru_position;
    };

    std::unordered_map<std::string, Entry> kv_store;
    KeyOrder lru_keys;
    std::size_t max_keys;

  public:
    explicit Database(std::size_t max_keys = 1024);

    void set(const std::string &key, const std::string &val);

    Result<std::optional<std::string>> get(const std::string &key);

    Result<std::int64_t> increment(const std::string &key);

    Result<std::int64_t> push_left(const std::string &key, Value value);
    Result<std::int64_t> push_right(const std::string &key, Value value);
    Result<std::optional<Value>> pop_left(const std::string &key);
    Result<std::optional<Value>> pop_right(const std::string &key);
    Result<std::int64_t> list_length(const std::string &key);

    bool del(const std::string &key);

  private:
    Result<ListPtr> get_or_create_list(const std::string &key);
    Result<ListPtr> find_list(const std::string &key);
    Entry &insert(const std::string &key, Value value);
    void touch(std::unordered_map<std::string, Entry>::iterator entry);
    void evict_if_full();
};

} // namespace reddish
