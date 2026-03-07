#include "database.hpp"

#include "error.hpp"

#include <algorithm>
#include <exception>
#include <limits>

namespace reddish
{

Database::Database(std::size_t max_keys) : max_keys(max_keys == 0 ? 1 : max_keys) {}

void Database::set(const std::string &key, const std::string &val)
{
    const auto entry = find_active(key);
    if (entry != kv_store.end())
    {
        entry->second.value = val;
        entry->second.expires_at.reset();
        touch(entry);
        return;
    }

    insert(key, val);
}

Result<std::optional<std::string>> Database::get(const std::string &key)
{
    auto it = find_active(key);
    if (it == Database::kv_store.end()) return std::nullopt;
    touch(it);

    if (const auto *string = std::get_if<std::string>(&it->second.value)) return *string;
    if (const auto *integer = std::get_if<std::int64_t>(&it->second.value))
        return std::to_string(*integer);
    return std::unexpected(Error{ErrorCode::wrong_type});
}

Result<std::int64_t> Database::increment(const std::string &key)
{
    auto it = find_active(key);
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

    if (std::holds_alternative<ListPtr>(it->second.value) ||
        std::holds_alternative<HashPtr>(it->second.value))
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
    if (values.empty()) del(key);
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
    if (values.empty()) del(key);
    return value;
}

Result<std::int64_t> Database::list_length(const std::string &key)
{
    auto list = find_list(key);
    if (!list) return std::unexpected(list.error());
    if (*list == nullptr) return 0;

    return static_cast<std::int64_t>((*list)->values.size());
}

Result<std::int64_t> Database::hash_set(const std::string &key, const std::string &field,
                                        Value value)
{
    auto hash = get_or_create_hash(key);
    if (!hash) return std::unexpected(hash.error());

    const auto [entry, inserted] = (*hash)->fields.insert_or_assign(field, std::move(value));
    return inserted ? 1 : 0;
}

Result<std::optional<Value>> Database::hash_get(const std::string &key, const std::string &field)
{
    auto hash = find_hash(key);
    if (!hash) return std::unexpected(hash.error());
    if (*hash == nullptr) return std::nullopt;

    const auto value = (*hash)->fields.find(field);
    if (value == (*hash)->fields.end()) return std::nullopt;
    return value->second;
}

Result<bool> Database::hash_del(const std::string &key, const std::string &field)
{
    auto hash = find_hash(key);
    if (!hash) return std::unexpected(hash.error());
    if (*hash == nullptr) return false;

    const bool removed = (*hash)->fields.erase(field) > 0;
    if ((*hash)->fields.empty()) del(key);
    return removed;
}

Result<std::int64_t> Database::hash_length(const std::string &key)
{
    auto hash = find_hash(key);
    if (!hash) return std::unexpected(hash.error());
    if (*hash == nullptr) return 0;

    return static_cast<std::int64_t>((*hash)->fields.size());
}

Result<ListPtr> Database::get_or_create_list(const std::string &key)
{
    auto it = find_active(key);
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
    const auto it = find_active(key);
    if (it == kv_store.end()) return nullptr;
    touch(it);
    if (auto *list = std::get_if<ListPtr>(&it->second.value)) return *list;
    return std::unexpected(Error{ErrorCode::wrong_type});
}

Result<HashPtr> Database::get_or_create_hash(const std::string &key)
{
    auto it = find_active(key);
    if (it == kv_store.end())
    {
        insert(key, std::make_shared<Hash>());
        it = kv_store.find(key);
    }
    else
    {
        touch(it);
    }

    if (auto *hash = std::get_if<HashPtr>(&it->second.value)) return *hash;
    return std::unexpected(Error{ErrorCode::wrong_type});
}

Result<HashPtr> Database::find_hash(const std::string &key)
{
    const auto it = find_active(key);
    if (it == kv_store.end()) return nullptr;
    touch(it);
    if (auto *hash = std::get_if<HashPtr>(&it->second.value)) return *hash;
    return std::unexpected(Error{ErrorCode::wrong_type});
}

bool Database::del(const std::string &key)
{
    const auto entry = find_active(key);
    if (entry == kv_store.end()) return false;

    erase(entry);
    return true;
}

bool Database::expire(const std::string &key, std::int64_t seconds)
{
    const auto entry = find_active(key);
    if (entry == kv_store.end()) return false;

    if (seconds <= 0)
    {
        erase(entry);
        return true;
    }

    entry->second.expires_at = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    touch(entry);
    return true;
}

std::int64_t Database::ttl(const std::string &key)
{
    const auto entry = find_active(key);
    if (entry == kv_store.end()) return -2;
    touch(entry);
    if (!entry->second.expires_at) return -1;

    return std::max<std::int64_t>(0,
                                  std::chrono::duration_cast<std::chrono::seconds>(
                                      *entry->second.expires_at - std::chrono::steady_clock::now())
                                      .count());
}

void Database::remove_expired()
{
    const auto now = std::chrono::steady_clock::now();
    for (auto entry = kv_store.begin(); entry != kv_store.end();)
    {
        if (entry->second.expires_at && *entry->second.expires_at <= now)
        {
            lru_keys.erase(entry->second.lru_position);
            entry = kv_store.erase(entry);
        }
        else
        {
            ++entry;
        }
    }
}

Database::Entry &Database::insert(const std::string &key, Value value)
{
    evict_if_full();
    lru_keys.push_front(key);
    auto [entry, inserted] =
        kv_store.emplace(key, Entry{std::move(value), lru_keys.begin(), std::nullopt});
    return entry->second;
}

Database::Store::iterator Database::find_active(const std::string &key)
{
    const auto entry = kv_store.find(key);
    if (entry == kv_store.end()) return entry;

    if (entry->second.expires_at && *entry->second.expires_at <= std::chrono::steady_clock::now())
    {
        erase(entry);
        return kv_store.end();
    }

    return entry;
}

void Database::erase(Store::iterator entry)
{
    lru_keys.erase(entry->second.lru_position);
    kv_store.erase(entry);
}

void Database::touch(Store::iterator entry)
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
