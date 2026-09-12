#pragma once
#include "database.hpp"
#include "response.hpp"
#include "result.hpp"
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace reddish
{

struct Command
{
    std::string_view name;
    std::vector<std::string_view> arguments;
    std::size_t bytes_consumed = 0;
};

struct ServerStats
{
    std::size_t connected_clients;
    std::chrono::steady_clock::time_point started_at;
    std::uint64_t input_buffer_disconnects;
    std::uint64_t output_buffer_disconnects;
};

Result<Command> parse_command(std::string_view input);

Response execute_command(const Command &command, Database &database, const ServerStats &stats);

std::string encode_response(const Response &response);
std::size_t encoded_response_size(const Response &response);
void append_encoded_response(const Response &response, std::string &output);

} // namespace reddish
