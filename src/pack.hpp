#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace reddish::pack
{

// Packed element encoding shared by lists and hashes.
//
// Layout of one element:
//
//   tag byte
//     0b0LLLLLLL  string, length in the low 7 bits (0..127)
//     0b10______  signed 64-bit integer, 8-byte little-endian payload
//     0b11000000  string, 4-byte little-endian length follows
//   payload
//   backlen  reverse-readable variable integer holding the header+payload size
//
// `backlen` exists so that the last element of a packed run can be found
// without scanning from the front, which is what makes RPOP of a packed
// list an O(1) operation. It is stored most-significant group first with the
// continuation bit set on every group except the first, so it can be decoded
// by walking backwards from the final byte.

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

constexpr std::size_t max_string_header = 5; // tag byte + 4-byte length

// Bytes the backlen field occupies for a header+payload size of `size`.
std::size_t backlen_bytes(std::size_t size) noexcept;

// Total bytes one encoded element occupies, backlen included.
std::size_t encoded_size(std::string_view text) noexcept;
std::size_t encoded_size(std::int64_t value) noexcept;

// Header+payload only, with no trailing backlen. Hashes use this form because
// they only ever walk their packed run forwards, so the backward link that
// RPOP needs would be dead weight.
std::size_t body_size(std::string_view text) noexcept;
std::size_t body_size(std::int64_t value) noexcept;

// Writes one element and returns the position just past it.
char *encode(char *out, std::string_view text) noexcept;
char *encode(char *out, std::int64_t value) noexcept;
char *encode_body(char *out, std::string_view text) noexcept;
char *encode_body(char *out, std::int64_t value) noexcept;

// Header+payload bytes of the element starting at `p`, excluding its backlen.
std::size_t element_size(const char *p) noexcept;

// Total stride of the element starting at `p`, backlen included.
std::size_t stride(const char *p) noexcept;

// Reads the backlen field ending just before `end` and returns the header+payload
// size it records; add backlen_bytes() of the result to obtain the full stride.
std::size_t decode_backlen(const char *end) noexcept;

// Decodes the element starting at `p`. `text` aliases the buffer.
Element decode(const char *p) noexcept;

} // namespace reddish::pack
