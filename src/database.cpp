#include "database.hpp"

#include "error.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <tuple>
#include <type_traits>

namespace reddish
{

namespace
{

template <typename T> bool write_value(std::ofstream &output, const T &value)
{
    output.write(reinterpret_cast<const char *>(&value), sizeof(value));
    return output.good();
}

template <typename T> bool read_value(std::ifstream &input, T &value)
{
    input.read(reinterpret_cast<char *>(&value), sizeof(value));
    return input.good();
}

bool write_string(std::ofstream &output, const std::string &value)
{
    const auto size = static_cast<std::uint64_t>(value.size());
    output.write(reinterpret_cast<const char *>(&size), sizeof(size));
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    return output.good();
}

bool read_string(std::ifstream &input, std::string &value)
{
    constexpr std::uint64_t max_string_size = 64 * 1024 * 1024;

    std::uint64_t size = 0;
    if (!read_value(input, size) || size > max_string_size) return false;

    value.resize(static_cast<std::size_t>(size));
    input.read(value.data(), static_cast<std::streamsize>(size));
    return input.good();
}

bool write_database_value(std::ofstream &output, const Value &value)
{
    return std::visit(
        [&](const auto &data)
        {
            using Data = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<Data, std::string>)
            {
                return write_value(output, std::uint8_t{0}) && write_string(output, data);
            }
            else if constexpr (std::is_same_v<Data, std::int64_t>)
            {
                return write_value(output, std::uint8_t{1}) && write_value(output, data);
            }
            else if constexpr (std::is_same_v<Data, ListPtr>)
            {
                if (!write_value(output, std::uint8_t{2}) ||
                    !write_value(output, static_cast<std::uint64_t>(data->values.size())))
                    return false;
                for (const auto &item : data->values)
                    if (!write_database_value(output, item)) return false;
                return true;
            }
            else
            {
                if (!write_value(output, std::uint8_t{3}) ||
                    !write_value(output, static_cast<std::uint64_t>(data->fields.size())))
                    return false;
                for (const auto &[field, item] : data->fields)
                    if (!write_string(output, field) || !write_database_value(output, item))
                        return false;
                return true;
            }
        },
        value);
}

bool read_database_value(std::ifstream &input, Value &value)
{
    constexpr std::uint64_t max_collection_size = 1'000'000;

    std::uint8_t type = 0;
    if (!read_value(input, type)) return false;

    if (type == 0)
    {
        std::string string;
        if (!read_string(input, string)) return false;
        value = std::move(string);
        return true;
    }

    if (type == 1)
    {
        std::int64_t integer = 0;
        if (!read_value(input, integer)) return false;
        value = integer;
        return true;
    }

    std::uint64_t count = 0;
    if ((type != 2 && type != 3) || !read_value(input, count) || count > max_collection_size)
        return false;

    if (type == 2)
    {
        auto list = std::make_shared<List>();
        for (std::uint64_t index = 0; index < count; ++index)
        {
            Value item;
            if (!read_database_value(input, item)) return false;
            list->values.push_back(std::move(item));
        }
        value = std::move(list);
        return true;
    }

    auto hash = std::make_shared<Hash>();
    for (std::uint64_t index = 0; index < count; ++index)
    {
        std::string field;
        Value item;
        if (!read_string(input, field) || !read_database_value(input, item)) return false;
        hash->fields.emplace(std::move(field), std::move(item));
    }
    value = std::move(hash);
    return true;
}

std::size_t value_memory_bytes(const Value &value)
{
    return std::visit(
        [](const auto &data) -> std::size_t
        {
            using Data = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<Data, std::string>)
            {
                return sizeof(data) + data.size();
            }
            else if constexpr (std::is_same_v<Data, std::int64_t>)
            {
                return sizeof(data);
            }
            else if constexpr (std::is_same_v<Data, ListPtr>)
            {
                std::size_t bytes = sizeof(List);
                for (const auto &item : data->values)
                    bytes += value_memory_bytes(item);
                return bytes;
            }
            else
            {
                std::size_t bytes = sizeof(Hash);
                for (const auto &[field, item] : data->fields)
                    bytes += field.size() + value_memory_bytes(item);
                return bytes;
            }
        },
        value);
}

} // namespace

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
    values.pop_front();
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
            ++expired_keys;
        }
        else
        {
            ++entry;
        }
    }
}

Result<void> Database::dump_to_disk(const std::filesystem::path &path)
{
    remove_expired();

    const auto temporary_path = path.string() + ".tmp";
    std::ofstream output(temporary_path, std::ios::binary | std::ios::trunc);
    if (!output) return std::unexpected(Error{ErrorCode::disk_write_failed});

    constexpr std::uint64_t format_version = 1;
    if (!write_value(output, format_version) ||
        !write_value(output, static_cast<std::uint64_t>(kv_store.size())))
        return std::unexpected(Error{ErrorCode::disk_write_failed});

    const auto steady_now = std::chrono::steady_clock::now();
    const auto system_now = std::chrono::system_clock::now();
    for (const auto &[key, entry] : kv_store)
    {
        const auto expires_at =
            entry.expires_at ? std::chrono::duration_cast<std::chrono::milliseconds>(
                                   system_now.time_since_epoch() + (*entry.expires_at - steady_now))
                                   .count()
                             : -1;
        if (!write_string(output, key) || !write_value(output, expires_at) ||
            !write_database_value(output, entry.value))
            return std::unexpected(Error{ErrorCode::disk_write_failed});
    }

    output.close();
    if (!output) return std::unexpected(Error{ErrorCode::disk_write_failed});

    std::error_code error;
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary_path, path, error);
    if (error)
    {
        std::filesystem::remove(temporary_path, error);
        return std::unexpected(Error{ErrorCode::disk_write_failed});
    }

    snapshot_bytes = static_cast<std::size_t>(std::filesystem::file_size(path, error));
    if (error) return std::unexpected(Error{ErrorCode::disk_write_failed});
    last_dump_unix_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

    return {};
}

Result<void> Database::load_from_disk(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::unexpected(Error{ErrorCode::disk_read_failed});

    constexpr std::uint64_t format_version = 1;
    std::uint64_t version = 0;
    std::uint64_t count = 0;
    if (!read_value(input, version) || version != format_version || !read_value(input, count))
        return std::unexpected(Error{ErrorCode::disk_read_failed});
    if (count > max_keys) return std::unexpected(Error{ErrorCode::dump_exceeds_key_capacity});

    std::vector<std::tuple<std::string, std::int64_t, Value>> entries;
    entries.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0; index < count; ++index)
    {
        std::string key;
        std::int64_t expires_at = -1;
        Value value;
        if (!read_string(input, key) || !read_value(input, expires_at) ||
            !read_database_value(input, value))
            return std::unexpected(Error{ErrorCode::disk_read_failed});
        entries.emplace_back(std::move(key), expires_at, std::move(value));
    }

    char trailing_byte = 0;
    if (input.read(&trailing_byte, 1) || !input.eof())
        return std::unexpected(Error{ErrorCode::disk_read_failed});

    kv_store.clear();
    lru_keys.clear();

    const auto system_now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();
    for (auto &[key, expires_at, value] : entries)
    {
        if (expires_at >= 0 && expires_at <= system_now) continue;

        auto &entry = insert(key, std::move(value));
        if (expires_at >= 0)
            entry.expires_at = std::chrono::steady_clock::now() +
                               std::chrono::milliseconds(expires_at - system_now);
    }

    return {};
}

DatabaseStats Database::stats() const
{
    std::size_t memory_bytes = 0;
    for (const auto &[key, entry] : kv_store)
        memory_bytes += sizeof(Entry) + key.size() + value_memory_bytes(entry.value);

    return {kv_store.size(), memory_bytes, snapshot_bytes,
            evictions,       expired_keys, last_dump_unix_ms};
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
        ++expired_keys;
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

    const auto entry = kv_store.find(lru_keys.back());
    erase(entry);
    ++evictions;
}

} // namespace reddish
