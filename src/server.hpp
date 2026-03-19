#pragma once

#include "database.hpp"
#include "resp.hpp"
#include "socket.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace reddish
{

struct ServerConfig
{
    std::uint16_t port = 6379;
    std::size_t max_keys = 1024;
    std::chrono::seconds dump_interval = std::chrono::minutes(5);
    std::filesystem::path dump_path = "dump.reddish";
};

class Server
{
  public:
    explicit Server(ServerConfig config = {});
    ~Server();

    void start();
    void start(bool (*should_stop)());
    void stop();
    Result<void> load_from_disk(const std::filesystem::path &path);

  private:
    struct ClientInfo
    {
        Socket socket;
        std::string buffer;
        std::string output_buffer;
        bool connected = false;
        bool close_after_write = false;
    };

    SocketSystem socket_system;
    ServerConfig config;
    Database database;
    bool running;
    std::chrono::steady_clock::time_point started_at;
    Socket listen_socket;
    std::unordered_map<int, ClientInfo> clients;
    int next_client_id;
    std::uint64_t input_buffer_disconnects = 0;
    std::uint64_t output_buffer_disconnects = 0;

    static constexpr std::size_t max_input_buffer_size = 1024 * 1024;
    static constexpr std::size_t max_output_buffer_size = 1024 * 1024;
    static constexpr int shutdown_poll_timeout_ms = 100;

    void accept_connections();
    void handle_client(int client_id);
    void flush_client(int client_id);
    void remove_client(int client_id);
    bool queue_response(int client_id, const Response &response);
};

} // namespace reddish
