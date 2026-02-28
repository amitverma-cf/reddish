#pragma once

#include "database.hpp"
#include "resp.hpp"
#include "socket.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace reddish
{

class Server
{
  public:
    Server(std::uint16_t port = 6379);
    ~Server();

    void start();
    void stop();

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
    std::uint16_t port;
    Database database;
    bool running;
    Socket listen_socket;
    std::unordered_map<int, ClientInfo> clients;
    int next_client_id;

    static constexpr std::size_t max_input_buffer_size = 1024 * 1024;

    void accept_connections();
    void handle_client(int client_id);
    void flush_client(int client_id);
    void remove_client(int client_id);
};

} // namespace reddish
