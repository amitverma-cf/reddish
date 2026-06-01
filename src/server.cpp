#include "server.hpp"
#include "error.hpp"

#include <iostream>
#include <utility>
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

Server::Server(ServerConfig config)
    : config(std::move(config)), database(this->config.max_keys), running(false), next_client_id(0)
{
}

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
    started_at = std::chrono::steady_clock::now();
    listen_socket.create_tcp();
    listen_socket.bind(config.port);
    listen_socket.listen(128);
    listen_socket.set_nonblocking();

    std::cout << "Server listening on port " << config.port << '\n';
    auto next_dump = std::chrono::steady_clock::now() + config.dump_interval;

    while (running && !should_stop())
    {
        std::vector<SocketPollRequest> requests{{&listen_socket, false}};
        for (auto &[client_id, client] : clients)
            if (client.connected)
                requests.push_back(
                    {&client.socket, client.output_offset < client.output_buffer.size()});

        const auto events = socket_system.wait_for_events(requests, shutdown_poll_timeout_ms);
        database.remove_expired();
        if (std::chrono::steady_clock::now() >= next_dump)
        {
            const auto dump = database.dump_to_disk(config.dump_path);
            if (!dump) throw dump.error();
            next_dump = std::chrono::steady_clock::now() + config.dump_interval;
        }
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

    if (running)
    {
        const auto dump = database.dump_to_disk(config.dump_path);
        stop();
        if (!dump) throw dump.error();
        return;
    }

    stop();
}

void Server::stop()
{
    running = false;
    clients.clear();
    listen_socket.close();
}

Result<void> Server::load_from_disk(const std::filesystem::path &path)
{
    return database.load_from_disk(path);
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
            if (client->second.buffer.size() - client->second.input_offset > max_input_buffer_size)
            {
                ++input_buffer_disconnects;
                if (!queue_response(client_id, Response{ErrorResponse{std::string(
                                                   error_message(ErrorCode::request_too_large))}}))
                    return;
                client->second.buffer.clear();
                client->second.input_offset = 0;
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

    while (client->second.input_offset < client->second.buffer.size())
    {
        const auto input =
            std::string_view(client->second.buffer).substr(client->second.input_offset);
        const auto command = parse_command(input);
        if (!command)
        {
            if (is_incomplete_resp_error(command.error().code())) return;

            if (!queue_response(client_id,
                                Response{ErrorResponse{
                                    std::string(error_message(ErrorCode::protocol_error_prefix)) +
                                    std::string(error_message(command.error().code()))}}))
                return;
            client->second.buffer.clear();
            client->second.close_after_write = true;
            return;
        }

        const ServerStats stats{clients.size(),
                                std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::steady_clock::now() - started_at)
                                    .count(),
                                input_buffer_disconnects, output_buffer_disconnects};
        if (!queue_response(client_id, execute_command(*command, database, stats))) return;
        client->second.input_offset += command->bytes_consumed;
        compact_buffer(client->second.buffer, client->second.input_offset);
    }
}

void Server::flush_client(int client_id)
{
    auto client = clients.find(client_id);
    if (client == clients.end()) return;

    while (client->second.output_offset < client->second.output_buffer.size())
    {
        const int bytes_sent = client->second.socket.send(
            client->second.output_buffer.data() + client->second.output_offset,
            client->second.output_buffer.size() - client->second.output_offset);
        if (bytes_sent > 0)
        {
            client->second.output_offset += static_cast<std::size_t>(bytes_sent);
            compact_buffer(client->second.output_buffer, client->second.output_offset);
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

bool Server::queue_response(int client_id, const Response &response)
{
    const auto client = clients.find(client_id);
    if (client == clients.end()) return false;

    const auto encoded = encode_response(response);
    if (client->second.output_buffer.size() - client->second.output_offset + encoded.size() >
        max_output_buffer_size)
    {
        ++output_buffer_disconnects;
        remove_client(client_id);
        return false;
    }

    client->second.output_buffer += encoded;
    return true;
}

void Server::compact_buffer(std::string &buffer, std::size_t &offset)
{
    if (offset == buffer.size())
    {
        buffer.clear();
        offset = 0;
    }
    else if (offset >= compact_buffer_threshold && offset * 2 >= buffer.size())
    {
        buffer.erase(0, offset);
        offset = 0;
    }
}

} // namespace reddish
