#include "error.hpp"
#include "server.hpp"
#include <charconv>
#include <csignal>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>

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

} // namespace

int main(int argc, char *argv[])
{
    try
    {
        std::signal(SIGINT, request_stop);
        std::signal(SIGTERM, request_stop);

        reddish::ServerConfig config;
        std::optional<std::filesystem::path> dump_path;
        const auto parse_positive_number = [](std::string_view input) -> std::optional<std::size_t>
        {
            std::size_t value = 0;
            const auto [position, error] =
                std::from_chars(input.data(), input.data() + input.size(), value);
            if (error != std::errc{} || position != input.data() + input.size() || value == 0)
                return std::nullopt;
            return value;
        };
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument = argv[index];
            if (argument == "--load-dump")
            {
                if (++index == argc)
                {
                    std::cerr << "Usage: reddish [port] [--max-keys <count>] [--dump-interval "
                                 "<seconds>] [--dump-path <path>] [--load-dump <path>]\n";
                    return 1;
                }
                dump_path = argv[index];
                continue;
            }
            if (argument == "--max-keys" || argument == "--dump-interval" ||
                argument == "--dump-path")
            {
                if (++index == argc)
                {
                    std::cerr << "Usage: reddish [port] [--max-keys <count>] [--dump-interval "
                                 "<seconds>] [--dump-path <path>] [--load-dump <path>]\n";
                    return 1;
                }
                if (argument == "--dump-path")
                {
                    config.dump_path = argv[index];
                    continue;
                }

                const auto value = parse_positive_number(argv[index]);
                if (!value)
                {
                    std::cerr << "Usage: reddish [port] [--max-keys <count>] [--dump-interval "
                                 "<seconds>] [--dump-path <path>] [--load-dump <path>]\n";
                    return 1;
                }
                if (argument == "--max-keys") config.max_keys = *value;
                else config.dump_interval = std::chrono::seconds(*value);
                continue;
            }

            const auto parsed_port = parse_positive_number(argument);
            if (!parsed_port || *parsed_port > std::numeric_limits<std::uint16_t>::max())
            {
                std::cerr << "Usage: reddish [port] [--max-keys <count>] [--dump-interval "
                             "<seconds>] [--dump-path <path>] [--load-dump <path>]\n";
                return 1;
            }
            config.port = static_cast<std::uint16_t>(*parsed_port);
        }

        reddish::Server server(config);
        if (dump_path && std::filesystem::exists(*dump_path))
        {
            const auto loaded = server.load_from_disk(*dump_path);
            if (!loaded) throw loaded.error();
        }
        else if (dump_path)
        {
            std::cerr << "Warning: " << dump_path->string()
                      << " was not found; starting with an empty database.\n";
        }
        server.start(should_stop);
    }
    catch (const reddish::Error &error)
    {
        std::cerr << "Server error: " << error.what() << '\n';
        return 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Unexpected server error: " << error.what() << '\n';
        return 1;
    }
}
