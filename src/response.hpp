#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace reddish
{

struct SimpleString
{
    std::string value;
};

struct BulkString
{
    std::string value;
};

struct BulkStringView
{
    std::string_view value;
};

struct ErrorResponse
{
    std::string message;
};

struct Integer
{
    std::int64_t value;
};

struct Null
{
};

struct Response;

struct ResponseArray
{
    std::vector<Response> values;
};

struct Response
{
    using Data = std::variant<SimpleString, ErrorResponse, Integer, BulkString, BulkStringView,
                              Null, ResponseArray>;

    Data data;
};
} // namespace reddish
