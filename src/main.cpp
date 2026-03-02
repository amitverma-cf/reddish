#include "error.hpp"
#include "server.hpp"
#include <csignal>
#include <exception>
#include <iostream>

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

int main()
{
    try
    {
        std::signal(SIGINT, request_stop);
        std::signal(SIGTERM, request_stop);

        reddish::Server server(6379);
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
