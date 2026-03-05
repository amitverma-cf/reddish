#include "database.hpp"

#include "error.hpp"

#include <exception>
#include <limits>

namespace reddish
{

Database::Database(std::size_t max_keys) : max_keys(max_keys == 0 ? 1 : max_keys) {}

void Database::set(const std::string &key, const std::string &val)
{
    const auto entry = kv_store.find(key);
    if (entry != kv_store.end())
    {
        entry->second.value = val;
        touch(entry);
        return;
    }

    insert(key, val);
}

Result<std::optional<std::string>> Database::get(const std::string &key)
{
    auto it = Database::kv_store.find(key);
    if (it == Database::kv_store.end()) return std::nullopt;
    touch(it);

    if (const auto *string = std::get_if<std::string>(&it->second.value)) return *string;
    if (const auto *integer = std::get_if<std::int64_t>(&it->second.value))
        return std::to_string(*integer);
    return std::unexpected(Error{ErrorCode::wrong_type});
}

Result<std::int64_t> Database::increment(const std::string &key)
{
    auto it = kv_store.find(key);
    if (it == kv_store.end())
    {
        insert(key, std::int64_t{1});
        return 1;
    }
    touch(it);

    if (auto *integer = std::get_if<std::int64_t>(&it->second.value))
    {
        if (*integer == std::numeric_limits<std::int64_t>::max())
            return std::unexpected(Error{ErrorCode::value_not_integer});
        ++*integer;
        return *integer;
    }

    if (std::holds_alternative<ListPtr>(it->second.value))
        return std::unexpected(Error{ErrorCode::wrong_type});

    try
    {
        std::size_t consumed = 0;
        const auto integer = std::stoll(std::get<std::string>(it->second.value), &consumed);
        if (consumed != std::get<std::string>(it->second.value).size())
            return std::unexpected(Error{ErrorCode::value_not_integer});
        if (integer == std::numeric_limits<std::int64_t>::max())
            return std::unexpected(Error{ErrorCode::value_not_integer});

        it->second.value = integer + 1;
        return integer + 1;
    }
    catch (const std::exception &)
    {
        return std::unexpected(Error{ErrorCode::value_not_integer});
    }
}

Result<std::int64_t> Database::push_left(const std::string &key, Value value)
{
    auto list = get_or_create_list(key);
    if (!list) return std::unexpected(list.error());

    (*list)->values.insert((*list)->values.begin(), std::move(value));
    return static_cast<std::int64_t>((*list)->values.size());
}

Result<std::int64_t> Database::push_right(const std::string &key, Value value)
{
    auto list = get_or_create_list(key);
    if (!list) return std::unexpected(list.error());

    (*list)->values.push_back(std::move(value));
    return static_cast<std::int64_t>((*list)->values.size());
}

Result<std::optional<Value>> Database::pop_left(const std::string &key)
{
    auto list = find_list(key);
    if (!list) return std::unexpected(list.error());
    if (*list == nullptr) return std::nullopt;

    auto &values = (*list)->values;
    if (values.empty()) return std::nullopt;

    Value value = std::move(values.front());
    values.erase(values.begin());
    if (values.empty()) kv_store.erase(key);
    return value;
}

Result<std::optional<Value>> Database::pop_right(const std::string &key)
{
    auto list = find_list(key);
    if (!list) return std::unexpected(list.error());
    if (*list == nullptr) return std::nullopt;

    auto &values = (*list)->values;
    if (values.empty()) return std::nullopt;

    Value value = std::move(values.back());
    values.pop_back();
    if (values.empty()) kv_store.erase(key);
    return value;
}

Result<std::int64_t> Database::list_length(const std::string &key)
{
    auto list = find_list(key);
    if (!list) return std::unexpected(list.error());
    if (*list == nullptr) return 0;

    return static_cast<std::int64_t>((*list)->values.size());
}

Result<ListPtr> Database::get_or_create_list(const std::string &key)
{
    auto it = kv_store.find(key);
    if (it == kv_store.end())
    {
        insert(key, std::make_shared<List>());
        it = kv_store.find(key);
    }
    else
    {
        touch(it);
    }

    if (auto *list = std::get_if<ListPtr>(&it->second.value)) return *list;
    return std::unexpected(Error{ErrorCode::wrong_type});
}

Result<ListPtr> Database::find_list(const std::string &key)
{
    const auto it = kv_store.find(key);
    if (it == kv_store.end()) return nullptr;
    touch(it);
    if (auto *list = std::get_if<ListPtr>(&it->second.value)) return *list;
    return std::unexpected(Error{ErrorCode::wrong_type});
}

bool Database::del(const std::string &key)
{
    const auto entry = kv_store.find(key);
    if (entry == kv_store.end()) return false;

    lru_keys.erase(entry->second.lru_position);
    kv_store.erase(entry);
    return true;
}

Database::Entry &Database::insert(const std::string &key, Value value)
{
    evict_if_full();
    lru_keys.push_front(key);
    auto [entry, inserted] = kv_store.emplace(key, Entry{std::move(value), lru_keys.begin()});
    return entry->second;
}

void Database::touch(std::unordered_map<std::string, Entry>::iterator entry)
{
    lru_keys.splice(lru_keys.begin(), lru_keys, entry->second.lru_position);
    entry->second.lru_position = lru_keys.begin();
}

void Database::evict_if_full()
{
    if (kv_store.size() < max_keys) return;

    const std::string &key = lru_keys.back();
    kv_store.erase(key);
    lru_keys.pop_back();
}

} // namespace reddish
