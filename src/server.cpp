#include "server.hpp"
#include "error.hpp"

#include <iostream>
#include <vector>

namespace reddish
{

namespace
{

bool is_incomplete_resp_error(ErrorCode code)
{
    return code == ErrorCode::missing_crlf || code == ErrorCode::missing_bulk_string_prefix ||
           code == ErrorCode::incomplete_bulk_string_data ||
           code == ErrorCode::missing_bulk_string_terminator;
}

} // namespace

Server::Server(std::uint16_t port) : port(port), running(false), next_client_id(0) {}

Server::~Server()
{
    stop();
}

void Server::start()
{
    start([] { return false; });
}

void Server::start(bool (*should_stop)())
{
    running = true;
    listen_socket.create_tcp();
    listen_socket.bind(port);
    listen_socket.listen(128);
    listen_socket.set_nonblocking();

    std::cout << "Server listening on port " << port << '\n';

    while (running && !should_stop())
    {
        std::vector<SocketPollRequest> requests{{&listen_socket, false}};
        for (auto &[client_id, client] : clients)
            if (client.connected)
                requests.push_back({&client.socket, !client.output_buffer.empty()});

        const auto events = socket_system.wait_for_events(requests, shutdown_poll_timeout_ms);
        for (const auto &event : events)
        {
            if (event.socket == &listen_socket)
            {
                if (event.error) throw Error{ErrorCode::listening_socket_failed};
                if (event.readable) accept_connections();
                continue;
            }

            int client_id = -1;
            for (const auto &[id, client] : clients)
            {
                if (&client.socket == event.socket)
                {
                    client_id = id;
                    break;
                }
            }
            if (client_id < 0) continue;

            if (event.readable) handle_client(client_id);
            if (!clients.contains(client_id)) continue;

            if (event.writable) flush_client(client_id);
            if (!clients.contains(client_id)) continue;

            if (event.error) remove_client(client_id);
        }
    }

    stop();
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
    while (true)
    {
        const int bytes_received = client->second.socket.receive(buffer, sizeof(buffer));
        if (bytes_received > 0)
        {
            client->second.buffer.append(buffer, bytes_received);
            if (client->second.buffer.size() > max_input_buffer_size)
            {
                client->second.output_buffer += encode_response(Response{
                    ErrorResponse{std::string(error_message(ErrorCode::request_too_large))}});
                client->second.buffer.clear();
                client->second.close_after_write = true;
                return;
            }
            continue;
        }

        if (bytes_received == 0)
        {
            remove_client(client_id);
            return;
        }

        if (client->second.socket.would_block()) break;
        remove_client(client_id);
        return;
    }

    while (!client->second.buffer.empty())
    {
        const auto command = parse_command(client->second.buffer);
        if (!command)
        {
            if (is_incomplete_resp_error(command.error().code())) return;

            client->second.output_buffer += encode_response(Response{
                ErrorResponse{std::string(error_message(ErrorCode::protocol_error_prefix)) +
                              std::string(error_message(command.error().code()))}});
            client->second.buffer.clear();
            client->second.close_after_write = true;
            return;
        }

        client->second.output_buffer += encode_response(execute_command(*command, database));
        client->second.buffer.erase(0, command->bytes_consumed);
    }
}

void Server::flush_client(int client_id)
{
    auto client = clients.find(client_id);
    if (client == clients.end()) return;

    while (!client->second.output_buffer.empty())
    {
        const int bytes_sent = client->second.socket.send(client->second.output_buffer.data(),
                                                          client->second.output_buffer.size());
        if (bytes_sent > 0)
        {
            client->second.output_buffer.erase(0, bytes_sent);
            continue;
        }

        if (bytes_sent < 0 && client->second.socket.would_block()) return;
        remove_client(client_id);
        return;
    }

    if (client->second.close_after_write) remove_client(client_id);
}

void Server::remove_client(int client_id)
{
    if (!clients.contains(client_id)) return;
    clients.erase(client_id);
    std::cout << "Client disconnected: " << client_id << '\n';
}

} // namespace reddish
