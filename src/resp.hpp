#pragma once

#include <expected>
#include <string>
#include <vector>

namespace reddish
{

struct Command
{
    std::string name;
    std::vector<std::string> arguments;
};

std::expected<Command, std::string> parse_command(const std::string &input);

} // namespace reddish
