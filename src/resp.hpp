#pragma once

#include <expected>
#include <string>
#include <vector>

namespace reddish
{

std::expected<std::vector<std::string>, std::string> parse_command(const std::string &input);

} // namespace reddish
