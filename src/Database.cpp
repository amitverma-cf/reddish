#include "Database.hpp"

namespace reddish
{

void Database::set(const std::string &key, const std::string &value)
{
    kv_store_[key] = value;
}

std::optional<std::string> Database::get(const std::string &key) const
{
    const auto item = kv_store_.find(key);
    if (item == kv_store_.end()) return std::nullopt;
    return item->second;
}

bool Database::del(const std::string &key)
{
    return kv_store_.erase(key) > 0;
}

} // namespace reddish
