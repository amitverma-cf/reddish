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

        std::uint16_t port = 6379;
        std::optional<std::filesystem::path> dump_path;
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument = argv[index];
            if (argument == "--load-dump")
            {
                if (++index == argc)
                {
                    std::cerr << "Usage: reddish [port] [--load-dump <path>]\n";
                    return 1;
                }
                dump_path = argv[index];
                continue;
            }

            unsigned int parsed_port = 0;
            const auto [position, error] =
                std::from_chars(argument.data(), argument.data() + argument.size(), parsed_port);
            if (error != std::errc{} || position != argument.data() + argument.size() ||
                parsed_port == 0 || parsed_port > std::numeric_limits<std::uint16_t>::max())
            {
                std::cerr << "Usage: reddish [port] [--load-dump <path>]\n";
                return 1;
            }
            port = static_cast<std::uint16_t>(parsed_port);
        }

        reddish::Server server(port);
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
