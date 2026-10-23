#pragma once
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace reddish
{
enum class WalSync
{
    everysec,
    always,
    none
};
enum class WalOp : unsigned char
{
    set = 0,
    increment = 1,
    push_left = 2,
    push_right = 3,
    pop_left = 4,
    pop_right = 5,
    hash_set = 6,
    hash_del = 7,
    expire_at = 8,
    del = 9,
    integer_set = 10
};

class Wal
{
  public:
    using Replay = std::function<void(WalOp, const std::vector<std::string_view> &)>;
    Wal(const std::filesystem::path &path, WalSync sync, const Replay &replay);
    ~Wal();
    Wal(const Wal &) = delete;
    Wal &operator=(const Wal &) = delete;
    void append(WalOp op, std::initializer_list<std::string_view> args);
    void flush();
    void begin_batch();
    void end_batch();
    void compact(const std::function<void(Wal &)> &write);
    void reserve_record(std::size_t bytes);
    std::size_t bytes() const noexcept
    {
        return bytes_.load();
    }
    bool failed() const noexcept
    {
        return failed_.load();
    }

  private:
    void flush_locked();
    std::FILE *file_ = nullptr;
    std::FILE *lock_file_ = nullptr;
    std::filesystem::path path_;
    std::array<char, 64 * 1024> file_buffer_{};
    WalSync sync_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::jthread flusher_;
    std::string buffer_;
    std::atomic<std::size_t> bytes_{0};
    std::atomic<bool> failed_{false};
    bool dirty_ = false;
    bool defer_sync_ = false;
};
}
