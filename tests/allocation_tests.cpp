#include "database.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace
{

thread_local std::ptrdiff_t fail_after = -1;
void check(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
struct LogFile
{
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("reddish-allocation-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".wal");
    ~LogFile()
    {
        std::error_code error;
        std::filesystem::remove(path, error);
        auto lock = path;
        lock += ".lock";
        std::filesystem::remove(lock, error);
    }
};

void verify_set(reddish::Database &db, int scenario, bool success, const std::string &value)
{
    if (success)
    {
        auto got = db.get("target");
        check(got && *got && **got == value, "SET result differs from requested value");
        check(db.ttl("target") == -1, "successful SET retained TTL");
    }
    else if (scenario == 0 || scenario == 4) check(!*db.get("target"), "failed SET inserted a key");
    else if (scenario == 2)
    {
        auto got = db.hash_get("target", "field");
        check(got && *got && std::get<std::string>(**got) == std::string(70000, 'h'),
              "failed SET destroyed the original hash");
        check(db.ttl("target") >= 0, "failed SET cleared hash TTL");
    }
    else
    {
        const std::string original = scenario == 3 ? std::string(70000, 's') : "original";
        auto got = db.get("target");
        check(got && *got && **got == original, "failed SET changed original value");
        check(db.ttl("target") >= 0, "failed SET cleared string TTL");
    }
    if (scenario == 4)
        for (int index = 0; index < 16; ++index)
            check(**db.get(std::to_string(index)) == "v", "rehash lost an existing key");
}

void test_set(int scenario, int mode)
{
    bool succeeded = false;
    for (std::ptrdiff_t allocation = 0; allocation < 64 && !succeeded; ++allocation)
    {
        LogFile log;
        const std::string value = scenario == 3 ? "s" : std::string(70000, 'h');
        {
            reddish::Database db;
            if (mode >= 0) db.open_wal(log.path, static_cast<reddish::WalSync>(mode));
            if (scenario == 1) db.set("target", "original");
            if (scenario == 2)
                check(db.hash_set_text("target", "field", value).has_value(), "hash setup failed");
            if (scenario == 3) db.set("target", std::string(70000, 's'));
            if (scenario >= 1 && scenario <= 3) check(db.expire("target", 60), "TTL setup failed");
            if (scenario == 4)
                for (int index = 0; index < 16; ++index)
                    db.set(std::to_string(index), "v");
            std::string_view input = value;
            if (scenario == 2) input = (**db.hash_field("target", "field")).text;
            if (scenario == 3) input = (**db.value_view("target")).text.substr(0, 1);
            const auto bytes = db.stats().wal_bytes;
            fail_after = allocation;
            try
            {
                db.set("target", input);
                fail_after = -1;
                succeeded = true;
            }
            catch (const std::bad_alloc &)
            {
                fail_after = -1;
            }
            fail_after = -1;
            verify_set(db, scenario, succeeded, value);
            if (!succeeded) check(db.stats().wal_bytes == bytes, "failed SET appended to WAL");
            db.flush_wal();
        }
        if (mode >= 0)
        {
            reddish::Database recovered;
            recovered.open_wal(log.path);
            verify_set(recovered, scenario, succeeded, value);
        }
    }
    check(succeeded, "SET allocation sweep did not reach success");
}

void test_pop(bool front, bool single, int mode)
{
    bool finished = false;
    const std::string large(70000, 'p');
    for (std::ptrdiff_t allocation = 0; allocation < 64 && !finished; ++allocation)
    {
        LogFile log;
        bool succeeded = false;
        {
            reddish::Database db;
            if (mode >= 0) db.open_wal(log.path, static_cast<reddish::WalSync>(mode));
            check(db.push_right_text("target", large).has_value(), "list setup failed");
            if (!single)
            {
                auto pushed = front ? db.push_right_text("target", "remaining")
                                    : db.push_left_text("target", "remaining");
                check(pushed.has_value(), "second list element setup failed");
            }
            check(db.expire("target", 60), "list TTL setup failed");
            const auto bytes = db.stats().wal_bytes;
            fail_after = allocation;
            try
            {
                auto popped = front ? db.pop_left("target") : db.pop_right("target");
                finished = fail_after >= 0;
                fail_after = -1;
                check(popped && *popped && std::get<std::string>(**popped) == large,
                      "pop returned the wrong value");
                succeeded = true;
            }
            catch (const std::bad_alloc &)
            {
                fail_after = -1;
            }
            fail_after = -1;
            const auto expected = (single ? 1 : 2) - (succeeded ? 1 : 0);
            check(db.list_length("target") == expected, "allocation failure lost a list element");
            check(expected == 0 ? db.ttl("target") == -2 : db.ttl("target") >= 0,
                  "pop lost list TTL");
            if (!succeeded) check(db.stats().wal_bytes == bytes, "failed pop appended to WAL");
            db.flush_wal();
        }
        if (mode >= 0)
        {
            reddish::Database recovered;
            recovered.open_wal(log.path);
            check(recovered.list_length("target") == (single ? 1 : 2) - (succeeded ? 1 : 0),
                  "replayed pop differs from live database");
            if (!succeeded)
            {
                auto popped = front ? recovered.pop_left("target") : recovered.pop_right("target");
                check(popped && *popped && std::get<std::string>(**popped) == large,
                      "failed pop changed the recovered element");
            }
            if (!single)
            {
                auto popped = recovered.pop_left("target");
                check(popped && *popped && std::get<std::string>(**popped) == "remaining",
                      "pop damaged the remaining element");
            }
        }
    }
    check(finished, "pop allocation sweep did not reach success");
}
}

void *operator new(std::size_t size)
{
    if (fail_after >= 0 && fail_after-- == 0)
    {
        fail_after = -1;
        throw std::bad_alloc();
    }
    if (void *memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete(void *memory) noexcept
{
    std::free(memory);
}
void operator delete[](void *memory) noexcept
{
    std::free(memory);
}
void operator delete(void *memory, std::size_t) noexcept
{
    std::free(memory);
}
void operator delete[](void *memory, std::size_t) noexcept
{
    std::free(memory);
}

void *operator new(std::size_t size, std::align_val_t alignment)
{
    const auto align = std::max<std::size_t>(static_cast<std::size_t>(alignment), alignof(void *));
    auto *raw = static_cast<char *>(::operator new(size + align + sizeof(void *)));
    const auto address =
        (reinterpret_cast<std::uintptr_t>(raw + sizeof(void *)) + align - 1) & ~(align - 1);
    auto *result = reinterpret_cast<void *>(address);
    reinterpret_cast<void **>(result)[-1] = raw;
    return result;
}
void *operator new[](std::size_t size, std::align_val_t alignment)
{
    return ::operator new(size, alignment);
}
void operator delete(void *memory, std::align_val_t) noexcept
{
    if (memory) ::operator delete(reinterpret_cast<void **>(memory)[-1]);
}
void operator delete[](void *memory, std::align_val_t alignment) noexcept
{
    ::operator delete(memory, alignment);
}
void operator delete(void *memory, std::size_t, std::align_val_t alignment) noexcept
{
    ::operator delete(memory, alignment);
}
void operator delete[](void *memory, std::size_t, std::align_val_t alignment) noexcept
{
    ::operator delete(memory, alignment);
}

int main()
{
    try
    {
        for (int mode = -1; mode < 3; ++mode)
        {
            for (int scenario = 0; scenario < 5; ++scenario)
                test_set(scenario, mode);
            for (bool front : {false, true})
                for (bool single : {false, true})
                    test_pop(front, single, mode);
        }
        std::cout << "SET and list pop allocation failure sweeps passed\n";
    }
    catch (const std::exception &error)
    {
        fail_after = -1;
        std::cerr << error.what() << '\n';
        return 1;
    }
}
