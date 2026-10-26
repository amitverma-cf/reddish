#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

namespace
{
bool fail(const char *operation)
{
    const auto *selected = std::getenv("REDDISH_FAULT");
    const auto *trigger = std::getenv("REDDISH_FAULT_TRIGGER");
    if (!selected || !trigger || std::strcmp(selected, operation) || ::access(trigger, F_OK))
        return false;
    static std::atomic<unsigned> count{0};
    const auto *nth = std::getenv("REDDISH_FAULT_NTH");
    if (++count != (nth ? std::strtoul(nth, nullptr, 10) : 1)) return false;
    if (::unlink(trigger)) return false;
    errno = EIO;
    return true;
}
bool wal_file(int fd)
{
    const auto *prefix = std::getenv("REDDISH_FAULT_WAL");
    if (!prefix) return false;
    char path[64], target[4096];
    std::snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    const auto size = ::readlink(path, target, sizeof(target) - 1);
    if (size < 0) return false;
    target[size] = 0;
    return std::strncmp(target, prefix, std::strlen(prefix)) == 0;
}
}

extern "C" std::size_t fwrite(const void *data, std::size_t size, std::size_t count,
                              std::FILE *file)
{
    static const auto real = reinterpret_cast<decltype(&fwrite)>(dlsym(RTLD_NEXT, "fwrite"));
    if (wal_file(fileno(file)) && fail("fwrite"))
    {
        errno = ENOSPC;
        return real(data, size, count / 2, file);
    }
    return real(data, size, count, file);
}
extern "C" int fflush(std::FILE *file)
{
    static const auto real = reinterpret_cast<decltype(&fflush)>(dlsym(RTLD_NEXT, "fflush"));
    if (file && wal_file(fileno(file)) && fail("fflush")) return EOF;
    return real(file);
}
extern "C" int fdatasync(int fd)
{
    static const auto real = reinterpret_cast<decltype(&fdatasync)>(dlsym(RTLD_NEXT, "fdatasync"));
    if (wal_file(fd) && fail("fdatasync")) return -1;
    return real(fd);
}
extern "C" int fsync(int fd)
{
    static const auto real = reinterpret_cast<decltype(&fsync)>(dlsym(RTLD_NEXT, "fsync"));
    if (fail("fsync")) return -1;
    return real(fd);
}
extern "C" int rename(const char *from, const char *to) noexcept
{
    static const auto real = reinterpret_cast<decltype(&rename)>(dlsym(RTLD_NEXT, "rename"));
    if (fail("crash-before-rename")) _exit(99);
    if (fail("rename")) return -1;
    const int result = real(from, to);
    if (!result && fail("crash-after-rename")) _exit(99);
    return result;
}
