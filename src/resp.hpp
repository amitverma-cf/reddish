#pragma once

#include "response.hpp"

#include <expected>
#include <string>
#include <vector>

namespace reddish
{

class Database;

struct Command
{
    std::string name;
    std::vector<std::string> arguments;
};

std::expected<Command, std::string> parse_command(const std::string &input);
Response execute_command(const Command &command, Database &database);
std::string encode_response(const Response &response);

} // namespace reddish
