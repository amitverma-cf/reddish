#pragma once

#include "error.hpp"

#include <expected>

namespace reddish
{

template <typename T> using Result = std::expected<T, Error>;

} // namespace reddish
