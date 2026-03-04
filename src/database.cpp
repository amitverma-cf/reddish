#include "database.hpp"

#include "error.hpp"

#include <exception>
#include <limits>

namespace reddish
{

void Database::set(const std::string &key, const std::string &val)
{
    Database::kv_store[key] = val;
}

Result<std::optional<std::string>> Database::get(const std::string &key) const
{
    auto it = Database::kv_store.find(key);
    if (it == Database::kv_store.end()) return std::nullopt;

    if (const auto *string = std::get_if<std::string>(&it->second)) return *string;
    if (const auto *integer = std::get_if<std::int64_t>(&it->second))
        return std::to_string(*integer);
    return std::unexpected(Error{ErrorCode::wrong_type});
}

Result<std::int64_t> Database::increment(const std::string &key)
{
    auto it = kv_store.find(key);
    if (it == kv_store.end())
    {
        kv_store.emplace(key, std::int64_t{1});
        return 1;
    }

    if (auto *integer = std::get_if<std::int64_t>(&it->second))
    {
        if (*integer == std::numeric_limits<std::int64_t>::max())
            return std::unexpected(Error{ErrorCode::value_not_integer});
        ++*integer;
        return *integer;
    }

    if (std::holds_alternative<ListPtr>(it->second))
        return std::unexpected(Error{ErrorCode::wrong_type});

    try
    {
        std::size_t consumed = 0;
        const auto integer = std::stoll(std::get<std::string>(it->second), &consumed);
        if (consumed != std::get<std::string>(it->second).size())
            return std::unexpected(Error{ErrorCode::value_not_integer});
        if (integer == std::numeric_limits<std::int64_t>::max())
            return std::unexpected(Error{ErrorCode::value_not_integer});

        it->second = integer + 1;
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

Result<std::int64_t> Database::list_length(const std::string &key) const
{
    auto list = find_list(key);
    if (!list) return std::unexpected(list.error());
    if (*list == nullptr) return 0;

    return static_cast<std::int64_t>((*list)->values.size());
}

Result<ListPtr> Database::get_or_create_list(const std::string &key)
{
    auto [it, inserted] = kv_store.try_emplace(key, std::make_shared<List>());
    if (auto *list = std::get_if<ListPtr>(&it->second)) return *list;
    return std::unexpected(Error{ErrorCode::wrong_type});
}

Result<ListPtr> Database::find_list(const std::string &key)
{
    const auto it = kv_store.find(key);
    if (it == kv_store.end()) return nullptr;
    if (auto *list = std::get_if<ListPtr>(&it->second)) return *list;
    return std::unexpected(Error{ErrorCode::wrong_type});
}

Result<ListPtr> Database::find_list(const std::string &key) const
{
    const auto it = kv_store.find(key);
    if (it == kv_store.end()) return nullptr;
    if (const auto *list = std::get_if<ListPtr>(&it->second)) return *list;
    return std::unexpected(Error{ErrorCode::wrong_type});
}

bool Database::del(const std::string &key)
{
    return Database::kv_store.erase(key) > 0;
}

} // namespace reddish
