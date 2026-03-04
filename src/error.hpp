#pragma once

#include <exception>
#include <string_view>

namespace reddish
{

enum class ErrorCode
{
    empty_resp_input,
    expected_resp_array_prefix,
    missing_crlf,
    invalid_resp_array_length,
    empty_resp_array,
    missing_bulk_string_prefix,
    expected_bulk_string_prefix,
    invalid_bulk_string_length,
    incomplete_bulk_string_data,
    missing_bulk_string_terminator,
    wrong_argument_count,
    unknown_command,
    protocol_error_prefix,
    value_not_integer,
    wrong_type,
    request_too_large,
    socket_startup_failed,
    socket_poll_failed,
    socket_creation_failed,
    socket_bind_failed,
    socket_listen_failed,
    socket_accept_failed,
    socket_nonblocking_failed,
    listening_socket_failed,
};

std::string_view error_message(ErrorCode code);

class Error final : public std::exception
{
  public:
    explicit Error(ErrorCode code) : code_(code) {}

    ErrorCode code() const
    {
        return code_;
    }
    const char *what() const noexcept override;

  private:
    ErrorCode code_;
};

} // namespace reddish
