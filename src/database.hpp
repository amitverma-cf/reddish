#pragma once
#include <optional>
#include <string>
#include <unordered_map>

namespace reddish
{

class Database
{
  private:
    std::unordered_map<std::string, std::string> kv_store;

  public:
    void set(const std::string &key, const std::string &val);

    std::optional<std::string> get(const std::string &key) const;

    bool del(const std::string &key);
};

} // namespace reddish
