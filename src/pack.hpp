#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace reddish::pack
{

enum class Kind : std::uint8_t
{
    string,
    integer,
};

struct Element
{
    Kind kind = Kind::string;
    std::string_view text;
    std::int64_t integer = 0;

    bool is_integer() const noexcept
    {
        return kind == Kind::integer;
    }
};

constexpr std::size_t max_string_header = 5;

std::size_t backlen_bytes(std::size_t size) noexcept;

std::size_t encoded_size(std::string_view text) noexcept;
std::size_t encoded_size(std::int64_t value) noexcept;

std::size_t body_size(std::string_view text) noexcept;
std::size_t body_size(std::int64_t value) noexcept;

char *encode(char *out, std::string_view text) noexcept;
char *encode(char *out, std::int64_t value) noexcept;
char *encode_body(char *out, std::string_view text) noexcept;
char *encode_body(char *out, std::int64_t value) noexcept;

std::size_t element_size(const char *p) noexcept;

std::size_t stride(const char *p) noexcept;

std::size_t decode_backlen(const char *end) noexcept;

Element decode(const char *p) noexcept;
}
