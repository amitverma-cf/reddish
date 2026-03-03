#pragma once
#include "result.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

namespace reddish
{

class Database
{
  private:
    using Value = std::variant<std::string, std::int64_t>;

    std::unordered_map<std::string, Value> kv_store;

  public:
    void set(const std::string &key, const std::string &val);

    std::optional<std::string> get(const std::string &key) const;

    Result<std::int64_t> increment(const std::string &key);

    bool del(const std::string &key);
};

} // namespace reddish
