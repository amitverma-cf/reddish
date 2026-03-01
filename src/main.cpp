#include "error.hpp"
#include "server.hpp"
#include <exception>
#include <iostream>

int main()
{
    try
    {
        reddish::Server server(6379);
        server.start();
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
