#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <csignal>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <catch_amalgamated.hpp>

#include "database.hpp"
#include "socket.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

using namespace reddish;

namespace
{

std::uint16_t next_port = 26000;

class ServerProcess
{
  public:
    ServerProcess(std::uint16_t port, const std::filesystem::path &dump_path)
    {
#ifdef _WIN32
        const std::wstring command = L"\"" + std::filesystem::path(REDDISH_EXECUTABLE).wstring() +
                                     L"\" " + std::to_wstring(port) + L" --load-dump \"" +
                                     dump_path.wstring() + L"\"";
        std::wstring mutable_command = command;
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        if (!CreateProcessW(nullptr, mutable_command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                            nullptr, &startup, &process_))
            throw std::runtime_error("could not start reddish");
#else
        process_ = fork();
        if (process_ < 0) throw std::runtime_error("could not start reddish");
        if (process_ == 0)
        {
            const auto port_text = std::to_string(port);
            execl(REDDISH_EXECUTABLE, REDDISH_EXECUTABLE, port_text.c_str(), "--load-dump",
                  dump_path.c_str(), nullptr);
            _exit(127);
        }
#endif
    }

    ~ServerProcess()
    {
#ifdef _WIN32
        if (process_.hProcess)
        {
            TerminateProcess(process_.hProcess, 0);
            WaitForSingleObject(process_.hProcess, 1000);
            CloseHandle(process_.hThread);
            CloseHandle(process_.hProcess);
        }
#else
        if (process_ > 0)
        {
            kill(process_, SIGINT);
            waitpid(process_, nullptr, 0);
        }
#endif
    }

  private:
#ifdef _WIN32
    PROCESS_INFORMATION process_{};
#else
    pid_t process_ = -1;
#endif
};

std::string get_loaded_value(std::uint16_t port)
{
    for (int attempt = 0; attempt < 50; ++attempt)
    {
#ifdef _WIN32
        const SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket == INVALID_SOCKET) throw std::runtime_error("could not create client socket");
#else
        const int socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socket < 0) throw std::runtime_error("could not create client socket");
#endif
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        if (::connect(socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == 0)
        {
            constexpr std::string_view request = "*2\r\n$3\r\nGET\r\n$4\r\nname\r\n";
            ::send(socket, request.data(), static_cast<int>(request.size()), 0);
            char response[64];
            const auto received = ::recv(socket, response, static_cast<int>(sizeof(response)), 0);
#ifdef _WIN32
            closesocket(socket);
#else
            ::close(socket);
#endif
            if (received > 0) return std::string(response, static_cast<std::size_t>(received));
        }
        else
        {
#ifdef _WIN32
            closesocket(socket);
#else
            ::close(socket);
#endif
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    throw std::runtime_error("reddish did not accept a connection");
}

} // namespace

TEST_CASE("main loads a requested dump before accepting TCP commands")
{
    SocketSystem socket_system;
    const auto dump_path = std::filesystem::temp_directory_path() / "reddish-main-test.reddish";
    std::error_code error;
    std::filesystem::remove(dump_path, error);

    Database database;
    database.set("name", "loaded");
    REQUIRE(database.dump_to_disk(dump_path));

    const auto port = next_port++;
    ServerProcess process(port, dump_path);
    CHECK(get_loaded_value(port) == "$6\r\nloaded\r\n");

    std::filesystem::remove(dump_path, error);
}
