#include "pack.hpp"

#include <bit>
#include <cstring>

namespace reddish::pack
{

namespace
{

constexpr std::uint8_t integer_tag = 0x80;     // 0b10______
constexpr std::uint8_t long_string_tag = 0xC0; // 0b11______

std::uint32_t load_u32(const char *p) noexcept
{
    std::uint32_t value = 0;
    std::memcpy(&value, p, sizeof(value));
    if constexpr (std::endian::native == std::endian::big) value = std::byteswap(value);
    return value;
}

void store_u32(char *p, std::uint32_t value) noexcept
{
    if constexpr (std::endian::native == std::endian::big) value = std::byteswap(value);
    std::memcpy(p, &value, sizeof(value));
}

std::uint64_t load_u64(const char *p) noexcept
{
    std::uint64_t value = 0;
    std::memcpy(&value, p, sizeof(value));
    if constexpr (std::endian::native == std::endian::big) value = std::byteswap(value);
    return value;
}

void store_u64(char *p, std::uint64_t value) noexcept
{
    if constexpr (std::endian::native == std::endian::big) value = std::byteswap(value);
    std::memcpy(p, &value, sizeof(value));
}

// Header + payload size of the element at `p`.
std::size_t header_and_payload(const char *p) noexcept
{
    const auto tag = static_cast<std::uint8_t>(p[0]);
    if ((tag & 0x80) == 0) return 1 + (tag & 0x7F);
    if ((tag & 0xC0) == integer_tag) return 1 + sizeof(std::int64_t);
    return max_string_header + load_u32(p + 1);
}

} // namespace

std::size_t backlen_bytes(std::size_t size) noexcept
{
    if (size <= 127) return 1;
    if (size < 16384) return 2;
    if (size < 2097152) return 3;
    if (size < 268435456) return 4;
    return 5;
}

std::size_t encoded_size(std::string_view text) noexcept
{
    const std::size_t header = text.size() <= 127 ? 1 : max_string_header;
    const std::size_t body = header + text.size();
    return body + backlen_bytes(body);
}

std::size_t encoded_size(std::int64_t value) noexcept
{
    (void)value;
    constexpr std::size_t body = 1 + sizeof(std::int64_t);
    return body + backlen_bytes(body);
}

std::size_t body_size(std::string_view text) noexcept
{
    return (text.size() <= 127 ? 1 : max_string_header) + text.size();
}

std::size_t body_size(std::int64_t value) noexcept
{
    (void)value;
    return 1 + sizeof(std::int64_t);
}

namespace
{

// Writes the trailing backlen describing a header+payload size of `size`.
// The first byte holds the most significant group and has its continuation bit
// clear; every later group has it set, so the field decodes backwards.
void encode_backlen(char *out, std::size_t size) noexcept
{
    if (size <= 127)
    {
        out[0] = static_cast<char>(size);
        return;
    }
    if (size < 16384)
    {
        out[0] = static_cast<char>(size >> 7);
        out[1] = static_cast<char>((size & 0x7F) | 0x80);
        return;
    }
    if (size < 2097152)
    {
        out[0] = static_cast<char>(size >> 14);
        out[1] = static_cast<char>(((size >> 7) & 0x7F) | 0x80);
        out[2] = static_cast<char>((size & 0x7F) | 0x80);
        return;
    }
    if (size < 268435456)
    {
        out[0] = static_cast<char>(size >> 21);
        out[1] = static_cast<char>(((size >> 14) & 0x7F) | 0x80);
        out[2] = static_cast<char>(((size >> 7) & 0x7F) | 0x80);
        out[3] = static_cast<char>((size & 0x7F) | 0x80);
        return;
    }
    out[0] = static_cast<char>(size >> 28);
    out[1] = static_cast<char>(((size >> 21) & 0x7F) | 0x80);
    out[2] = static_cast<char>(((size >> 14) & 0x7F) | 0x80);
    out[3] = static_cast<char>(((size >> 7) & 0x7F) | 0x80);
    out[4] = static_cast<char>((size & 0x7F) | 0x80);
}

} // namespace

char *encode(char *out, std::string_view text) noexcept
{
    char *cursor = out;
    if (text.size() <= 127)
    {
        *cursor++ = static_cast<char>(text.size());
    }
    else
    {
        *cursor++ = static_cast<char>(long_string_tag);
        store_u32(cursor, static_cast<std::uint32_t>(text.size()));
        cursor += sizeof(std::uint32_t);
    }

    if (!text.empty()) std::memcpy(cursor, text.data(), text.size());
    cursor += text.size();

    const std::size_t body = static_cast<std::size_t>(cursor - out);
    encode_backlen(cursor, body);
    return cursor + backlen_bytes(body);
}

char *encode(char *out, std::int64_t value) noexcept
{
    out[0] = static_cast<char>(integer_tag);
    store_u64(out + 1, static_cast<std::uint64_t>(value));

    constexpr std::size_t body = 1 + sizeof(std::int64_t);
    encode_backlen(out + body, body);
    return out + body + backlen_bytes(body);
}

std::size_t element_size(const char *p) noexcept
{
    return header_and_payload(p);
}

char *encode_body(char *out, std::string_view text) noexcept
{
    char *cursor = out;
    if (text.size() <= 127)
    {
        *cursor++ = static_cast<char>(text.size());
    }
    else
    {
        *cursor++ = static_cast<char>(long_string_tag);
        store_u32(cursor, static_cast<std::uint32_t>(text.size()));
        cursor += sizeof(std::uint32_t);
    }
    if (!text.empty()) std::memcpy(cursor, text.data(), text.size());
    return cursor + text.size();
}

char *encode_body(char *out, std::int64_t value) noexcept
{
    out[0] = static_cast<char>(integer_tag);
    store_u64(out + 1, static_cast<std::uint64_t>(value));
    return out + 1 + sizeof(std::int64_t);
}

std::size_t stride(const char *p) noexcept
{
    const std::size_t body = header_and_payload(p);
    return body + backlen_bytes(body);
}

std::size_t decode_backlen(const char *end) noexcept
{
    const char *cursor = end;
    std::size_t value = 0;
    std::size_t shift = 0;
    while (true)
    {
        --cursor;
        const auto byte = static_cast<std::uint8_t>(*cursor);
        value |= static_cast<std::size_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    return value;
}

Element decode(const char *p) noexcept
{
    const auto tag = static_cast<std::uint8_t>(p[0]);

    if ((tag & 0x80) == 0)
        return Element{Kind::string, std::string_view{p + 1, static_cast<std::size_t>(tag & 0x7F)},
                       0};

    if ((tag & 0xC0) == integer_tag)
        return Element{Kind::integer, {}, static_cast<std::int64_t>(load_u64(p + 1))};

    const auto length = load_u32(p + 1);
    return Element{Kind::string,
                   std::string_view{p + max_string_header, static_cast<std::size_t>(length)}, 0};
}

} // namespace reddish::pack
