#include <catch2/catch_test_macros.hpp>

#include "error.hpp"

#include <array>
#include <string_view>

using namespace reddish;

TEST_CASE("every error code has a message")
{
    constexpr std::array codes{
        ErrorCode::empty_resp_input,
        ErrorCode::expected_resp_array_prefix,
        ErrorCode::missing_crlf,
        ErrorCode::invalid_resp_array_length,
        ErrorCode::empty_resp_array,
        ErrorCode::missing_bulk_string_prefix,
        ErrorCode::expected_bulk_string_prefix,
        ErrorCode::invalid_bulk_string_length,
        ErrorCode::incomplete_bulk_string_data,
        ErrorCode::missing_bulk_string_terminator,
        ErrorCode::wrong_argument_count,
        ErrorCode::unknown_command,
        ErrorCode::protocol_error_prefix,
        ErrorCode::value_not_integer,
        ErrorCode::wrong_type,
        ErrorCode::unsupported_info_section,
        ErrorCode::request_too_large,
        ErrorCode::disk_write_failed,
        ErrorCode::disk_read_failed,
        ErrorCode::dump_exceeds_key_capacity,
        ErrorCode::socket_startup_failed,
        ErrorCode::socket_poll_failed,
        ErrorCode::socket_creation_failed,
        ErrorCode::socket_bind_failed,
        ErrorCode::socket_listen_failed,
        ErrorCode::socket_accept_failed,
        ErrorCode::socket_nonblocking_failed,
        ErrorCode::listening_socket_failed,
    };

    for (const auto code : codes)
    {
        const Error error{code};
        CHECK_FALSE(error_message(code).empty());
        CHECK(std::string_view{error.what()} == error_message(code));
    }
}
