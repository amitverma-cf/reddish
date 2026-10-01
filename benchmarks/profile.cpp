#include "database.hpp"
#include "resp.hpp"
#include <chrono>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#else
#define CALLGRIND_START_INSTRUMENTATION
#define CALLGRIND_STOP_INSTRUMENTATION
#endif
using namespace reddish;
int main(int argc, char **argv)
{
    const std::string shape = argc > 1 ? argv[1] : "mixed";
    const std::size_t iterations = argc > 2 ? std::stoull(argv[2]) : 1000000;
    Database db(20000);
    std::vector<std::string> keys;
    for (int i = 0; i < 10000; ++i)
    {
        keys.push_back("key:" + std::to_string(i));
        db.set(keys.back(), std::string(32, 'x'));
    }
    if (iterations == 0 || iterations > 1000000000 ||
        (shape != "reads" && shape != "writes" && shape != "mixed" && shape != "queue"))
    {
        std::cerr << "Usage: reddish_profile reads|writes|mixed|queue [positive iterations <= "
                     "1000000000]\n";
        return 1;
    }
    if (shape == "queue") (void)db.push_right_text("q", "x");
    std::string output;
    std::uint64_t checksum = 0;
    CALLGRIND_START_INSTRUMENTATION;
    const auto started = std::chrono::steady_clock::now();
    const auto cpu = std::clock();
    for (std::size_t i = 0; i < iterations; ++i)
    {
        if (shape == "queue")
        {
            (void)db.push_right_text("q", "x");
            (void)db.pop_left("q");
            ++checksum;
            continue;
        }
        const auto &key = keys[(i * 2654435761ULL) % keys.size()];
        const bool write = shape == "writes" || (shape == "mixed" && i % 2 == 0);
        Command command{write ? "SET" : "GET",
                        write ? ArgumentList{key, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"}
                              : ArgumentList{key}};
        output.clear();
        execute_into(command, db, {}, output);
        checksum += output.size();
    }
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    const double cpu_seconds = double(std::clock() - cpu) / CLOCKS_PER_SEC;
    CALLGRIND_STOP_INSTRUMENTATION;
    auto stats = db.stats();
    const auto operations = shape == "queue" ? iterations * 2 : iterations;
    std::cout << "shape,operations,wall_seconds,cpu_seconds,cpu_us_per_op,ops_per_sec,active_bytes,"
                 "reserved_bytes,checksum\n"
              << shape << ',' << operations << ',' << seconds << ',' << cpu_seconds << ','
              << cpu_seconds * 1000000 / operations << ',' << operations / seconds << ','
              << stats.approximate_memory_bytes << ',' << stats.reserved_memory_bytes << ','
              << checksum << '\n';
}
