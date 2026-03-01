#pragma once
#include "response.hpp"
#include "result.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace reddish
{

class Database;

struct Command
{
    std::string name;
    std::vector<std::string> arguments;
    std::size_t bytes_consumed = 0;
};

Result<Command> parse_command(const std::string &input);

Response execute_command(const Command &command, Database &database);

std::string encode_response(const Response &response);

} // namespace reddish
