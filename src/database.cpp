#include "database.hpp"

namespace reddish
{

void Database::set(const std::string &key, const std::string &val)
{
    Database::kv_store[key] = val;
}

std::optional<std::string> Database::get(const std::string &key) const
{
    auto it = Database::kv_store.find(key);
    if (it != Database::kv_store.end()) return it->second;
    return std::nullopt;
}

bool Database::del(const std::string &key)
{
    return Database::kv_store.erase(key) > 0;
}

} // namespace reddish
