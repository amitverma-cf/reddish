#include "Server.hpp"

#include "resp.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

Server::Server(std::uint16_t port) : port_(port)
{
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed");
}

Server::~Server()
{
    WSACleanup();
}

void Server::start()
{
    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) throw std::runtime_error("Failed to create TCP socket");

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port_);

    if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        closesocket(listener);
        throw std::runtime_error("Failed to bind TCP socket");
    }

    if (listen(listener, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(listener);
        throw std::runtime_error("Failed to listen on TCP socket");
    }

    std::cout << "Server listening on port " << port_ << '\n';

    while (true)
    {
        const SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        char buffer[4096];
        const int received = recv(client, buffer, sizeof(buffer), 0);
        if (received > 0)
        {
            const auto command = reddish::parse_command(std::string(buffer, received));
            const reddish::Response response =
                command ? reddish::execute_command(*command, database_)
                        : reddish::Response{reddish::ErrorResponse{command.error()}};
            const std::string encoded = reddish::encode_response(response);
            send(client, encoded.data(), static_cast<int>(encoded.size()), 0);
        }

        closesocket(client);
    }
}
