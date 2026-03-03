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

std::optional<std::string> Database::get(const std::string &key) const
{
    auto it = Database::kv_store.find(key);
    if (it == Database::kv_store.end()) return std::nullopt;

    if (const auto *string = std::get_if<std::string>(&it->second)) return *string;
    return std::to_string(std::get<std::int64_t>(it->second));
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

bool Database::del(const std::string &key)
{
    return Database::kv_store.erase(key) > 0;
}

} // namespace reddish
