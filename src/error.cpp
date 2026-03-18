#include "error.hpp"

namespace reddish
{

std::string_view error_message(ErrorCode code)
{
    switch (code)
    {
    case ErrorCode::empty_resp_input:
        return "Empty RESP input";
    case ErrorCode::expected_resp_array_prefix:
        return "Expected RESP array prefix '*'";
    case ErrorCode::missing_crlf:
        return "Missing CRLF";
    case ErrorCode::invalid_resp_array_length:
        return "Invalid RESP array length";
    case ErrorCode::empty_resp_array:
        return "RESP array contains no command";
    case ErrorCode::missing_bulk_string_prefix:
        return "Missing bulk string prefix";
    case ErrorCode::expected_bulk_string_prefix:
        return "Expected bulk string prefix '$'";
    case ErrorCode::invalid_bulk_string_length:
        return "Invalid RESP bulk string length";
    case ErrorCode::incomplete_bulk_string_data:
        return "RESP bulk string data is incomplete";
    case ErrorCode::missing_bulk_string_terminator:
        return "RESP bulk string is missing trailing CRLF";
    case ErrorCode::wrong_argument_count:
        return "ERR wrong number of arguments for";
    case ErrorCode::unknown_command:
        return "ERR unknown command";
    case ErrorCode::protocol_error_prefix:
        return "ERR Protocol error: ";
    case ErrorCode::value_not_integer:
        return "ERR value is not an integer or out of range";
    case ErrorCode::wrong_type:
        return "WRONGTYPE Operation against a key holding the wrong kind of value";
    case ErrorCode::unsupported_info_section:
        return "ERR unsupported INFO section";
    case ErrorCode::request_too_large:
        return "ERR request exceeds maximum buffer size";
    case ErrorCode::disk_write_failed:
        return "Failed to write database dump";
    case ErrorCode::disk_read_failed:
        return "Failed to read database dump";
    case ErrorCode::dump_exceeds_key_capacity:
        return "Dump contains more keys than --max-keys allows";
    case ErrorCode::socket_startup_failed:
        return "Failed to initialize socket system";
    case ErrorCode::socket_poll_failed:
        return "Socket event polling failed";
    case ErrorCode::socket_creation_failed:
        return "Failed to create TCP socket";
    case ErrorCode::socket_bind_failed:
        return "Failed to bind TCP socket";
    case ErrorCode::socket_listen_failed:
        return "Failed to listen on TCP socket";
    case ErrorCode::socket_accept_failed:
        return "Failed to accept TCP client";
    case ErrorCode::socket_nonblocking_failed:
        return "Failed to set socket as nonblocking";
    case ErrorCode::listening_socket_failed:
        return "Listening socket failed";
    }

    return "Unknown error";
}

const char *Error::what() const noexcept
{
    return error_message(code_).data();
}

} // namespace reddish
