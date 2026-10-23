#include "wal.hpp"
#include "error.hpp"
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#ifdef _WIN32
#include <io.h>
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace reddish
{
namespace
{
constexpr std::string_view magic = "RDWAL001";
constexpr std::uint32_t max_record = 128 * 1024 * 1024;
void take_lock(std::FILE *file)
{
#ifdef _WIN32
    OVERLAPPED lock{};
    lock.Offset = 0xffffffff;
    lock.OffsetHigh = 0x7fffffff;
    if (!LockFileEx(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file))),
                    LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &lock))
        throw Error{ErrorCode::disk_write_failed};
#else
    if (::flock(fileno(file), LOCK_EX | LOCK_NB) != 0 ||
        ::fcntl(fileno(file), F_SETFD, FD_CLOEXEC) != 0)
        throw Error{ErrorCode::disk_write_failed};
#endif
}
constexpr auto crc_table = []
{
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i)
    {
        auto c = i;
        for (int bit = 0; bit < 8; ++bit)
            c = (c >> 1) ^ ((c & 1) ? 0xedb88320U : 0U);
        table[i] = c;
    }
    return table;
}();
std::uint32_t crc(std::string_view bytes)
{
    std::uint32_t c = ~0U;
    for (unsigned char ch : bytes)
        c = crc_table[(c ^ ch) & 255] ^ (c >> 8);
    return ~c;
}
void put32(std::string &out, std::uint32_t n)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<char>(n >> (8 * i)));
}
std::uint32_t get32(const char *p)
{
    std::uint32_t n = 0;
    for (int i = 0; i < 4; ++i)
        n |= std::uint32_t(static_cast<unsigned char>(p[i])) << (8 * i);
    return n;
}
std::size_t recover(const std::filesystem::path &path, const Wal::Replay &replay)
{
    if (!std::filesystem::exists(path) || std::filesystem::file_size(path) == 0) return 0;
    std::ifstream input(path, std::ios::binary);
    char header[8];
    if (!input.read(header, 8) || std::string_view(header, 8) != magic)
        throw Error{ErrorCode::disk_read_failed};
    std::size_t valid = 8;
    bool torn = false;
    while (true)
    {
        input.read(header, 8);
        if (input.gcount() == 0 && input.eof()) break;
        if (input.gcount() != 8)
        {
            if (!input.eof()) throw Error{ErrorCode::disk_read_failed};
            torn = true;
            break;
        }
        const auto size = get32(header), checksum = get32(header + 4);
        if (size < 2 || size > max_record) throw Error{ErrorCode::disk_read_failed};
        std::string payload(size, '\0');
        if (!input.read(payload.data(), size))
        {
            if (!input.eof()) throw Error{ErrorCode::disk_read_failed};
            torn = true;
            break;
        }
        if (crc(payload) != checksum) throw Error{ErrorCode::disk_read_failed};
        const auto op = static_cast<WalOp>(static_cast<unsigned char>(payload[0]));
        const auto argc = static_cast<unsigned char>(payload[1]);
        std::vector<std::string_view> args;
        std::size_t pos = 2;
        for (unsigned i = 0; i < argc; ++i)
        {
            if (payload.size() - pos < 4) throw Error{ErrorCode::disk_read_failed};
            const auto n = get32(payload.data() + pos);
            pos += 4;
            if (n > payload.size() - pos) throw Error{ErrorCode::disk_read_failed};
            args.emplace_back(payload.data() + pos, n);
            pos += n;
        }
        if (pos != payload.size()) throw Error{ErrorCode::disk_read_failed};
        replay(op, args);
        valid += 8 + size;
    }
    input.close();

    if (torn) std::filesystem::resize_file(path, valid);
    return valid;
}
}

Wal::Wal(const std::filesystem::path &path, WalSync sync, const Replay &replay)
    : path_(path), sync_(sync)
{
    auto lock_path = path;
    lock_path += ".lock";
#ifdef _WIN32
    lock_file_ = _wfopen(lock_path.c_str(), L"abN");
#else
    lock_file_ = std::fopen(lock_path.c_str(), "ab");
#endif
    if (!lock_file_) throw Error{ErrorCode::disk_write_failed};
    try
    {
        take_lock(lock_file_);
#ifdef _WIN32
        file_ = _wfopen(path.c_str(), L"abN");
#else
        file_ = std::fopen(path.c_str(), "abe");
#endif
        if (!file_) throw Error{ErrorCode::disk_write_failed};
        take_lock(file_);
        const auto valid = recover(path, replay);
        if (std::setvbuf(file_, file_buffer_.data(), _IOFBF, file_buffer_.size()) != 0)
            throw Error{ErrorCode::disk_write_failed};
        bytes_ = valid;
        if (valid == 0)
        {
            if (std::fwrite(magic.data(), 1, magic.size(), file_) != magic.size())
                throw Error{ErrorCode::disk_write_failed};
            bytes_ = magic.size();
            dirty_ = true;
            flush_locked();
#ifndef _WIN32
            if (sync_ != WalSync::none)
            {
                auto parent = path.parent_path();
                if (parent.empty()) parent = ".";
                const int directory = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
                if (directory < 0) throw Error{ErrorCode::disk_write_failed};
                const int result = ::fsync(directory);
                ::close(directory);
                if (result != 0) throw Error{ErrorCode::disk_write_failed};
            }
#endif
        }
        flusher_ = std::jthread(
            [this](std::stop_token stop)
            {
                std::unique_lock guard(mutex_);
                while (!stop.stop_requested())
                {
                    wake_.wait_for(guard, std::chrono::seconds(1),
                                   [&] { return stop.stop_requested(); });
                    try
                    {
                        flush_locked();
                    }
                    catch (...)
                    {
                        failed_ = true;
                        return;
                    }
                }
            });
    }
    catch (...)
    {
        if (file_) std::fclose(file_);
        std::fclose(lock_file_);
        file_ = nullptr;
        lock_file_ = nullptr;
        throw;
    }
}
Wal::~Wal()
{
    flusher_.request_stop();
    wake_.notify_all();
    if (flusher_.joinable()) flusher_.join();
    if (file_)
    {
        try
        {
            flush_locked();
        }
        catch (...)
        {
            failed_ = true;
        }
        std::fclose(file_);
    }
    if (lock_file_) std::fclose(lock_file_);
}
void Wal::append(WalOp op, std::initializer_list<std::string_view> args)
{
    std::lock_guard guard(mutex_);
    if (failed_) throw Error{ErrorCode::disk_write_failed};
    std::size_t size = 2;
    for (auto arg : args)
    {
        if (arg.size() > max_record - 4 || size > max_record - arg.size() - 4)
            throw Error{ErrorCode::request_too_large};
        size += 4 + arg.size();
    }
    if (size > max_record || args.size() > 255) throw Error{ErrorCode::request_too_large};
    buffer_.clear();
    buffer_.reserve(size + 8);
    put32(buffer_, static_cast<std::uint32_t>(size));
    put32(buffer_, 0);
    buffer_.push_back(static_cast<char>(op));
    buffer_.push_back(static_cast<char>(args.size()));
    for (auto arg : args)
    {
        put32(buffer_, static_cast<std::uint32_t>(arg.size()));
        buffer_.append(arg);
    }
    const auto checksum = crc(std::string_view(buffer_).substr(8));
    for (int i = 0; i < 4; ++i)
        buffer_[4 + i] = static_cast<char>(checksum >> (8 * i));
    if (std::fwrite(buffer_.data(), 1, buffer_.size(), file_) != buffer_.size())
    {
        failed_ = true;
        throw Error{ErrorCode::disk_write_failed};
    }
    bytes_ += buffer_.size();
    dirty_ = true;
    if (sync_ == WalSync::always && !defer_sync_) flush_locked();
}
void Wal::reserve_record(std::size_t bytes)
{
    std::lock_guard guard(mutex_);
    if (bytes > max_record + 8) throw Error{ErrorCode::request_too_large};
    buffer_.reserve(bytes);
}
void Wal::flush_locked()
{
    if (failed_) throw Error{ErrorCode::disk_write_failed};
    if (!dirty_) return;
    if (std::fflush(file_) != 0)
    {
        failed_ = true;
        throw Error{ErrorCode::disk_write_failed};
    }
    if (sync_ != WalSync::none)
    {
#ifdef _WIN32
        const int result = _commit(_fileno(file_));
#else
        const int result = ::fdatasync(fileno(file_));
#endif
        if (result != 0)
        {
            failed_ = true;
            throw Error{ErrorCode::disk_write_failed};
        }
    }
    dirty_ = false;
}
void Wal::flush()
{
    std::lock_guard guard(mutex_);
    flush_locked();
}
void Wal::begin_batch()
{
    std::lock_guard guard(mutex_);
    defer_sync_ = sync_ == WalSync::always;
}
void Wal::end_batch()
{
    std::lock_guard guard(mutex_);
    defer_sync_ = false;
    if (sync_ == WalSync::always) flush_locked();
}

void Wal::compact(const std::function<void(Wal &)> &write)
{
    std::lock_guard guard(mutex_);
    if (failed_ || defer_sync_) throw Error{ErrorCode::disk_write_failed};
    auto temporary = path_;
    temporary +=
        ".compact-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    if (std::filesystem::exists(temporary)) throw Error{ErrorCode::disk_write_failed};
    auto temporary_lock = temporary;
    temporary_lock += ".lock";
    bool switching = false;
    try
    {
        std::size_t bytes;
        {
            Wal replacement(temporary, WalSync::always, [](WalOp, const auto &) {});
            replacement.begin_batch();
            write(replacement);
            replacement.end_batch();
            replacement.flush();
            bytes = replacement.bytes();
        }
        if (recover(temporary, [](WalOp, const auto &) {}) != bytes)
            throw Error{ErrorCode::disk_read_failed};
        flush_locked();
        switching = true;
        if (std::fclose(file_) != 0)
        {
            file_ = nullptr;
            throw Error{ErrorCode::disk_write_failed};
        }
        file_ = nullptr;
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path_.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw Error{ErrorCode::disk_write_failed};
        file_ = _wfopen(path_.c_str(), L"abN");
#else
        std::filesystem::rename(temporary, path_);
        auto parent = path_.parent_path();
        if (parent.empty()) parent = ".";
        const int directory = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (directory < 0) throw Error{ErrorCode::disk_write_failed};
        const int status = ::fsync(directory);
        ::close(directory);
        if (status != 0) throw Error{ErrorCode::disk_write_failed};
        file_ = std::fopen(path_.c_str(), "abe");
#endif
        if (!file_ || std::setvbuf(file_, file_buffer_.data(), _IOFBF, file_buffer_.size()) != 0)
            throw Error{ErrorCode::disk_write_failed};
        take_lock(file_);
        bytes_ = bytes;
        dirty_ = false;
        std::error_code error;
        std::filesystem::remove(temporary_lock, error);
    }
    catch (...)
    {
        if (switching) failed_ = true;
        std::error_code error;
        std::filesystem::remove(temporary, error);
        std::filesystem::remove(temporary_lock, error);
        throw;
    }
}
}
