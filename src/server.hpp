#pragma once

#include "database.hpp"
#include "resp.hpp"
#include "socket.hpp"
#include <string>
#include <unordered_map>

namespace reddish
{

class Server
{
  public:
    Server(uint16_t port = 6379);
    ~Server();

    void start();
    void stop();

  private:
    struct ClientInfo
    {
        Socket socket;
        std::string buffer;
        bool connected;

        ClientInfo() : connected(false) {}
    };

    SocketSystem socket_system;
    uint16_t port;
    Database database;
    bool running;

    Socket listen_socket;

    std::unordered_map<int, ClientInfo> clients;
    int next_client_id;

    void accept_connections();
    void handle_client(int client_id);
    void remove_client(int client_id);
};

} // namespace reddish
