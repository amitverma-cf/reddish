#include "server.hpp"

#include <iostream>
#include <vector>

namespace reddish
{

Server::Server(std::uint16_t port) : port(port), running(false), next_client_id(0) {}

Server::~Server()
{
    stop();
}

void Server::start()
{
    running = true;
    listen_socket.create_tcp();
    listen_socket.bind(port);
    listen_socket.listen(128);
    listen_socket.set_nonblocking();

    std::cout << "Server listening on port " << port << '\n';

    while (running)
    {
        std::vector<Socket *> watched_sockets{&listen_socket};
        for (auto &[client_id, client] : clients)
            if (client.connected) watched_sockets.push_back(&client.socket);

        const auto ready_sockets = socket_system.wait_for_readable(watched_sockets);
        for (Socket *ready_socket : ready_sockets)
        {
            if (ready_socket == &listen_socket)
            {
                accept_connections();
                continue;
            }

            for (const auto &[client_id, client] : clients)
            {
                if (&client.socket != ready_socket) continue;
                handle_client(client_id);
                break;
            }
        }
    }
}

void Server::stop()
{
    running = false;
    clients.clear();
    listen_socket.close();
}

void Server::accept_connections()
{
    while (true)
    {
        Socket client = listen_socket.accept();
        if (!client.valid()) return;

        client.set_nonblocking();
        const int client_id = next_client_id++;
        auto [entry, inserted] = clients.try_emplace(client_id);
        entry->second.socket = std::move(client);
        entry->second.connected = true;
        std::cout << "Client connected: " << client_id << '\n';
    }
}

void Server::handle_client(int client_id)
{
    auto client = clients.find(client_id);
    if (client == clients.end()) return;

    char buffer[4096];
    const int bytes_received = client->second.socket.receive(buffer, sizeof(buffer));
    if (bytes_received <= 0)
    {
        if (bytes_received < 0 && client->second.socket.would_block()) return;
        remove_client(client_id);
        return;
    }

    client->second.buffer.append(buffer, bytes_received);

    while (!client->second.buffer.empty())
    {
        const auto command = parse_command(client->second.buffer);
        if (!command) return;

        const Response response = execute_command(*command, database);
        const std::string encoded = encode_response(response);
        client->second.socket.send(encoded.data(), encoded.size());
        client->second.buffer.erase(0, command->bytes_consumed);
    }
}

void Server::remove_client(int client_id)
{
    if (!clients.contains(client_id)) return;
    clients.erase(client_id);
    std::cout << "Client disconnected: " << client_id << '\n';
}

} // namespace reddish
