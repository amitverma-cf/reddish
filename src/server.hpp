#pragma once

#include "database.hpp"
#include "resp.hpp"
#include "socket.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <vector>

namespace reddish
{

struct ServerConfig
{
    std::uint16_t port = 6379;
    std::size_t max_keys = 1024;
    std::filesystem::path wal_path;
    WalSync wal_sync = WalSync::everysec;
};

// The event loop runs bounded command batches. Maintenance owns physical expiry
// and exact LRU capacity eviction; WAL owns its periodic flush/sync thread.
// Every database batch has exclusive ownership. Capacity pressure waits for the
// maintenance thread at a batch boundary; there is no inline eviction.
class Server
{
  public:
    explicit Server(ServerConfig config = {});
    ~Server();

    void start();
    void start(bool (*should_stop)());
    void stop();

  private:
    struct ClientInfo
    {
        Socket socket;
        std::string buffer;
        std::size_t input_offset = 0;
        std::string output_buffer;
        std::size_t output_offset = 0;
        // Reused parse slots: a pipelined batch never allocates argument storage.
        std::vector<Command> batch;
        bool watching_write = false;
        bool close_after_write = false;
        bool half_closed = false;
        bool processing_queued = false;
    };

    SocketSystem socket_system;
    ServerConfig config;
    Database database;
    std::atomic<bool> running;
    std::chrono::steady_clock::time_point started_at;
    Socket listen_socket;
    std::unordered_map<int, ClientInfo> clients;
    int next_client_id;
    std::uint64_t input_buffer_disconnects = 0;
    std::uint64_t output_buffer_disconnects = 0;
    std::atomic<bool> maintenance_failed{false};
    std::mutex maintenance_mutex;
    std::condition_variable maintenance_wake;
    std::deque<int> pending_clients;

    static constexpr std::size_t max_input_buffer_size = 1024 * 1024;
    static constexpr std::size_t max_output_buffer_size = 1024 * 1024;
    static constexpr std::size_t read_chunk_size = 64 * 1024;
    static constexpr std::size_t batch_limit = 1024;
    static constexpr std::size_t compact_buffer_threshold = 64 * 1024;
    static constexpr int event_wait_timeout_ms = 50;
    // Keys handled per lock acquisition by the background threads. Keeping these
    // small bounds how long the maintenance thread can hold the lock.
    static constexpr std::size_t expiry_slice = 128;
    static constexpr std::size_t eviction_slice = 256;

    void accept_connections();
    void read_client(int client_id);
    void process_client(int client_id);
    void execute_batch(ClientInfo &client, std::size_t count, bool has_write);
    void flush_client(int client_id);
    void remove_client(int client_id);
    void close_connections();
    void request_write(int client_id);
    void maintenance_loop(std::stop_token token);
    static void compact_buffer(std::string &buffer, std::size_t &offset);
};

} // namespace reddish
