#include "error.hpp"
#include "server.hpp"
#include <charconv>
#include <csignal>
#include <iostream>
#include <limits>
#include <optional>

namespace
{
volatile std::sig_atomic_t stop_requested = 0;
void request_stop(int)
{
    stop_requested = 1;
}
bool should_stop()
{
    return stop_requested != 0;
}
constexpr const char *usage = "Usage: reddish [port] [--max-keys <count>] [--wal-path <path>] "
                              "[--wal-sync everysec|always|none] [--no-wal]\n";
}
int main(int argc, char *argv[])
{
    try
    {
        std::signal(SIGINT, request_stop);
        std::signal(SIGTERM, request_stop);
        reddish::ServerConfig config;
        config.wal_path = "reddish.wal";
        bool disable_wal = false;
        bool port_supplied = false;
        auto number = [](std::string_view text) -> std::optional<std::size_t>
        {
            std::size_t n = 0;
            auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), n);
            if (ec != std::errc{} || end != text.data() + text.size() || !n) return std::nullopt;
            return n;
        };
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view flag = argv[i];
            if (flag == "--no-wal")
            {
                disable_wal = true;
                continue;
            }
            if (flag == "--max-keys" || flag == "--wal-path" || flag == "--wal-sync")
            {
                if (++i == argc)
                {
                    std::cerr << usage;
                    return 1;
                }
                const std::string_view value = argv[i];
                if (flag == "--max-keys")
                {
                    auto n = number(value);
                    if (!n)
                    {
                        std::cerr << usage;
                        return 1;
                    }
                    config.max_keys = *n;
                }
                else if (flag == "--wal-path")
                {
                    if (value.empty())
                    {
                        std::cerr << usage;
                        return 1;
                    }
                    config.wal_path = argv[i];
                }
                else if (value == "always") config.wal_sync = reddish::WalSync::always;
                else if (value == "everysec") config.wal_sync = reddish::WalSync::everysec;
                else if (value == "none") config.wal_sync = reddish::WalSync::none;
                else
                {
                    std::cerr << usage;
                    return 1;
                }
            }
            else
            {
                auto port = number(flag);
                if (port_supplied || !port || *port > std::numeric_limits<std::uint16_t>::max())
                {
                    std::cerr << usage;
                    return 1;
                }
                config.port = static_cast<std::uint16_t>(*port);
                port_supplied = true;
            }
        }
        if (disable_wal) config.wal_path.clear();
        reddish::Server server(config);
        server.start(should_stop);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Server error: " << e.what() << '\n';
        return 1;
    }
}
