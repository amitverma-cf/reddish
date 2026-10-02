#pragma once
#include "database.hpp"
#include "result.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace reddish
{

// Argument list with inline storage for the common case.
//
// The previous implementation reserved a std::vector per parsed command, which
// meant one heap allocation and one free for every request. Every command this
// server implements takes at most three arguments, so keeping eight slots inline
// removes that allocation entirely and only the rare wide command spills.
class ArgumentList
{
  public:
    static constexpr std::size_t inline_capacity = 8;

    ArgumentList() = default;
    ArgumentList(std::initializer_list<std::string_view> values)
    {
        for (const auto value : values)
            push_back(value);
    }

    void clear() noexcept
    {
        size_ = 0;
        overflow_.clear();
    }

    void push_back(std::string_view value)
    {
        if (size_ < inline_capacity)
        {
            inline_[size_++] = value;
            return;
        }
        if (size_ == inline_capacity) overflow_.assign(inline_, inline_ + inline_capacity);
        overflow_.push_back(value);
        ++size_;
    }

    std::size_t size() const noexcept
    {
        return size_;
    }
    bool empty() const noexcept
    {
        return size_ == 0;
    }
    std::string_view operator[](std::size_t index) const noexcept
    {
        return data()[index];
    }

    const std::string_view *begin() const noexcept
    {
        return data();
    }
    const std::string_view *end() const noexcept
    {
        return data() + size_;
    }

  private:
    const std::string_view *data() const noexcept
    {
        return size_ <= inline_capacity ? inline_ : overflow_.data();
    }

    std::size_t size_ = 0;
    std::string_view inline_[inline_capacity];
    std::vector<std::string_view> overflow_;
};

enum class CommandId : std::uint8_t;

struct Command
{
    std::string_view name;
    ArgumentList arguments;
    std::size_t bytes_consumed = 0;
    // Cached by the event loop after parsing. Direct callers use the overload
    // that classifies the current name, so editing a Command cannot stale it.
    CommandId classified_id{};
};

struct ServerStats
{
    std::size_t connected_clients;
    std::chrono::steady_clock::time_point started_at;
    std::uint64_t input_buffer_disconnects;
    std::uint64_t output_buffer_disconnects;
};

enum class CommandId : std::uint8_t
{
    unknown,
    ping,
    info,
    get,
    set,
    increment,
    list_push_left,
    list_push_right,
    list_pop_left,
    list_pop_right,
    list_length,
    hash_set,
    hash_get,
    hash_del,
    hash_length,
    expire,
    ttl,
    del,
};

struct CommandInfo
{
    CommandId id = CommandId::unknown;
    // True for commands that can modify values. All batches take exclusive
    // ownership because reads also update exact LRU state.
    bool is_write = false;
};

// Case-insensitive command lookup by a folded name tag: one switch instead of a
// chain of string comparisons.
CommandInfo classify_command(std::string_view name) noexcept;

Result<Command> parse_command(std::string_view input);

// Fills a caller-owned Command so that a pipelined batch can reuse its argument
// storage instead of allocating per command.
Result<void> parse_command_into(std::string_view input, Command &command);

// Appends the RESP encoding of one command's reply to `output`.
void execute_into(const Command &command, Database &database, const ServerStats &stats,
                  std::string &output);
void execute_into(const Command &command, Database &database, const ServerStats &stats,
                  std::string &output, CommandId classified_id);

// Single pass reply writers. Every one of them appends directly to the output
// buffer, so a reply is never built twice.
void append_simple_string(std::string &output, std::string_view text);
void append_error(std::string &output, std::string_view text);
void append_integer(std::string &output, std::int64_t value);
void append_bulk_string(std::string &output, std::string_view text);
void append_null(std::string &output);
void append_array_header(std::string &output, std::int64_t count);

} // namespace reddish
