#include "server.hpp"
#include "error.hpp"

#include <iostream>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <stop_token>
#include <thread>
#include <utility>

namespace reddish
{

namespace
{

bool is_incomplete_resp_error(ErrorCode code)
{
    return code == ErrorCode::missing_crlf || code == ErrorCode::missing_bulk_string_prefix ||
           code == ErrorCode::incomplete_bulk_string_data;
}

}

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
    if (!config.wal_path.empty()) database.open_wal(config.wal_path, config.wal_sync);
    running = true;
    started_at = std::chrono::steady_clock::now();

    listen_socket.create_tcp();
    listen_socket.bind(config.port);
    listen_socket.listen(128);
    listen_socket.set_nonblocking();
    socket_system.watch(listen_socket, SocketSystem::listener_token, true, false);

    std::cout << "Server listening on port " << config.port << '\n';

    std::jthread maintenance([this](std::stop_token token) { maintenance_loop(token); });

    while (running && !should_stop())
    {
        if (maintenance_failed || database.wal_failed()) throw Error{ErrorCode::disk_write_failed};
        const auto queued = std::min<std::size_t>(pending_clients.size(), 64);
        for (std::size_t i = 0; i < queued; ++i)
        {
            const int id = pending_clients.front();
            pending_clients.pop_front();
            auto entry = clients.find(id);
            if (entry == clients.end()) continue;
            entry->second.processing_queued = false;
            process_client(id);
        }
        for (const auto &event :
             socket_system.wait(pending_clients.empty() ? event_wait_timeout_ms : 0))
        {
            if (event.token == SocketSystem::listener_token)
            {
                if (event.error) throw Error{ErrorCode::listening_socket_failed};
                if (event.readable) accept_connections();
                continue;
            }

            const int client_id = event.token;
            if (!clients.contains(client_id)) continue;

            if (event.readable) read_client(client_id);
            if (!clients.contains(client_id)) continue;

            if (event.writable) flush_client(client_id);
            if (!clients.contains(client_id)) continue;

            if (event.error) remove_client(client_id);
        }
    }

    maintenance.request_stop();
    maintenance_wake.notify_all();
    maintenance.join();
    database.flush_wal();
    stop();
    close_connections();
    if (maintenance_failed) throw Error{ErrorCode::disk_write_failed};
}

void Server::stop()
{
    running = false;
    maintenance_wake.notify_all();
}

void Server::close_connections()
{
    for (auto &[client_id, client] : clients)
        socket_system.unwatch(client.socket);
    clients.clear();
    socket_system.unwatch(listen_socket);
    listen_socket.close();
}

void Server::accept_connections()
{
    for (std::size_t accepted = 0; accepted < 64; ++accepted)
    {
        Socket client = listen_socket.accept();
        if (!client.valid()) return;

        client.set_nonblocking();
        client.set_nodelay();

        if (clients.size() >= 4096) continue;
        while (clients.contains(next_client_id))
            next_client_id =
                next_client_id == std::numeric_limits<int>::max() ? 0 : next_client_id + 1;
        const int client_id = next_client_id;
        next_client_id = next_client_id == std::numeric_limits<int>::max() ? 0 : next_client_id + 1;
        auto [entry, inserted] = clients.try_emplace(client_id);
        entry->second.socket = std::move(client);
        socket_system.watch(entry->second.socket, client_id, true, false);
        std::cout << "Client connected: " << client_id << '\n';
    }
}

void Server::read_client(int client_id)
{
    auto entry = clients.find(client_id);
    if (entry == clients.end()) return;
    ClientInfo &client = entry->second;

    if (client.half_closed) return;

    char chunk[read_chunk_size];
    while (true)
    {
        const int received = client.socket.receive(chunk, sizeof(chunk));
        if (received > 0)
        {
            client.buffer.append(chunk, static_cast<std::size_t>(received));
            if (client.buffer.size() - client.input_offset > max_input_buffer_size)
            {
                ++input_buffer_disconnects;
                try
                {
                    append_error(client.output_buffer, error_message(ErrorCode::request_too_large));
                }
                catch (const Error &)
                {
                    ++output_buffer_disconnects;
                    remove_client(client_id);
                    return;
                }
                client.buffer.clear();
                client.input_offset = 0;
                client.close_after_write = true;
                request_write(client_id);
                return;
            }
            continue;
        }

        if (received == 0)
        {

            client.half_closed = true;
            client.close_after_write = true;
            break;
        }

        if (client.socket.would_block()) break;
        remove_client(client_id);
        return;
    }

    process_client(client_id);
}

void Server::process_client(int client_id)
{
    auto entry = clients.find(client_id);
    if (entry == clients.end()) return;
    ClientInfo &client = entry->second;

    if (client.output_offset != 0)
    {
        client.output_buffer.erase(0, client.output_offset);
        client.output_offset = 0;
    }

    std::size_t count = 0;
    bool has_write = false;
    bool protocol_error = false;
    ErrorCode protocol_code = ErrorCode::empty_resp_input;

    while (count < batch_limit && client.input_offset < client.buffer.size())
    {
        if (count == client.batch.size()) client.batch.emplace_back();
        Command &command = client.batch[count];

        const auto input = std::string_view{client.buffer}.substr(client.input_offset);
        const auto parsed = parse_command_into(input, command);
        if (!parsed)
        {

            if (is_incomplete_resp_error(parsed.error().code())) break;
            protocol_error = true;
            protocol_code = parsed.error().code();
            break;
        }

        const auto classification = classify_command(command.name);
        command.classified_id = classification.id;
        if (classification.is_write) has_write = true;
        client.input_offset += command.bytes_consumed;
        ++count;
    }

    if (count != 0)
    {
        try
        {
            execute_batch(client, count, has_write);
        }
        catch (const Error &error)
        {
            if (error.code() != ErrorCode::request_too_large) throw;
            ++output_buffer_disconnects;
            remove_client(client_id);
            return;
        }
    }

    if (protocol_error)
    {
        try
        {
            append_error(client.output_buffer,
                         std::string(error_message(ErrorCode::protocol_error_prefix)) +
                             std::string(error_message(protocol_code)));
        }
        catch (const Error &)
        {
            ++output_buffer_disconnects;
            remove_client(client_id);
            return;
        }
        client.buffer.clear();
        client.input_offset = 0;
        client.close_after_write = true;
        request_write(client_id);
        return;
    }

    compact_buffer(client.buffer, client.input_offset);
    if (count == batch_limit && client.input_offset < client.buffer.size() &&
        !client.processing_queued)
    {
        client.processing_queued = true;
        pending_clients.push_back(client_id);
    }

    if (client.output_buffer.size() - client.output_offset > max_output_buffer_size)
    {
        ++output_buffer_disconnects;
        remove_client(client_id);
        return;
    }

    if (client.output_offset < client.output_buffer.size())
    {
        flush_client(client_id);
        auto entry = clients.find(client_id);
        if (entry != clients.end() &&
            entry->second.output_offset < entry->second.output_buffer.size())
            request_write(client_id);
    }
    else if (client.close_after_write && !client.processing_queued) remove_client(client_id);
}

void Server::execute_batch(ClientInfo &client, std::size_t count, bool has_write)
{
    const ServerStats stats{clients.size(), started_at, input_buffer_disconnects,
                            output_buffer_disconnects};

    {

        std::unique_lock guard(database.mutex());
        if (has_write) database.begin_wal_batch();
        try
        {
            for (std::size_t index = 0; index < count; ++index)
                execute_into(client.batch[index], database, stats, client.output_buffer,
                             client.batch[index].classified_id);
        }
        catch (...)
        {
            if (has_write) database.end_wal_batch();
            throw;
        }

        if (has_write) database.end_wal_batch();
    }

    if (database.over_capacity())
    {
        maintenance_wake.notify_all();
        std::unique_lock guard(maintenance_mutex);
        maintenance_wake.wait(guard,
                              [&] { return !database.over_capacity() || maintenance_failed; });
        if (maintenance_failed) throw Error{ErrorCode::disk_write_failed};
    }
}

void Server::request_write(int client_id)
{
    auto entry = clients.find(client_id);
    if (entry == clients.end()) return;
    ClientInfo &client = entry->second;
    if (client.watching_write) return;

    socket_system.watch(client.socket, client_id, !client.half_closed, true);
    client.watching_write = true;
}

void Server::flush_client(int client_id)
{
    auto entry = clients.find(client_id);
    if (entry == clients.end()) return;
    ClientInfo &client = entry->second;

    while (client.output_offset < client.output_buffer.size())
    {
        const int bytes_sent =
            client.socket.send(client.output_buffer.data() + client.output_offset,
                               client.output_buffer.size() - client.output_offset);
        if (bytes_sent > 0)
        {
            client.output_offset += static_cast<std::size_t>(bytes_sent);
            compact_buffer(client.output_buffer, client.output_offset);
            continue;
        }

        if (bytes_sent < 0 && client.socket.would_block()) break;
        remove_client(client_id);
        return;
    }

    if (client.output_offset < client.output_buffer.size()) return;

    if (client.watching_write)
    {
        socket_system.watch(client.socket, client_id, !client.half_closed, false);
        client.watching_write = false;
    }
    if (client.close_after_write && !client.processing_queued) remove_client(client_id);
}

void Server::remove_client(int client_id)
{
    const auto entry = clients.find(client_id);
    if (entry == clients.end()) return;

    socket_system.unwatch(entry->second.socket);
    clients.erase(entry);
    std::cout << "Client disconnected: " << client_id << '\n';
}

void Server::compact_buffer(std::string &buffer, std::size_t &offset)
{
    if (offset == buffer.size())
    {
        buffer.clear();
        if (buffer.capacity() > compact_buffer_threshold * 4) std::string{}.swap(buffer);
        offset = 0;
    }
    else if (offset >= compact_buffer_threshold && offset * 2 >= buffer.size())
    {
        buffer.erase(0, offset);
        offset = 0;
    }
}

void Server::maintenance_loop(std::stop_token token)
{
    try
    {
        while (!token.stop_requested())
        {
            if (database.maintenance_needed())
            {
                database.remove_expired(expiry_slice);
                if (database.over_capacity())
                    database.evict_to(database.max_keys(), eviction_slice);
                {
                    std::lock_guard guard(maintenance_mutex);
                    maintenance_wake.notify_all();
                }
            }
            std::unique_lock guard(maintenance_mutex);
            maintenance_wake.wait_for(
                guard, std::chrono::milliseconds(20),
                [&] { return token.stop_requested() || database.over_capacity(); });
        }
    }
    catch (...)
    {
        std::lock_guard guard(maintenance_mutex);
        maintenance_failed = true;
        maintenance_wake.notify_all();
    }
}

}
