#include "client.hpp"

namespace reddish::client
{

Client::Client(std::string host, std::uint16_t port) : host_(std::move(host)), port_(port) {}

Client::~Client()
{
    close();
}

std::expected<void, std::string> Client::connect()
{
    return socket_.connect(host_, port_);
}

std::expected<std::string, std::string> Client::execute(
    const std::vector<std::string_view> &command)
{
    std::string request = "*" + std::to_string(command.size()) + "\r\n";
    for (const auto part : command)
        request += "$" + std::to_string(part.size()) + "\r\n" + std::string(part) + "\r\n";

    const auto sent = socket_.send(request.data(), request.size());
    if (!sent) return std::unexpected(sent.error());

    char buffer[4096];
    const auto count = socket_.receive(buffer, sizeof(buffer));
    if (!count) return std::unexpected(count.error());
    return std::string(buffer, *count);
}

void Client::close()
{
    socket_.close();
}

} // namespace reddish::client
