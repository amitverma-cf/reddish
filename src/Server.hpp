#pragma once

#include "Database.hpp"

#include <cstdint>

class Server
{
  public:
    explicit Server(std::uint16_t port = 6379);
    ~Server();

    void start();

  private:
    std::uint16_t port_;
    reddish::Database database_;
};
