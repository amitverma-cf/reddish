#include "resp.hpp"
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    reddish::Command command;
    const std::string_view input(reinterpret_cast<const char *>(data), size);
    const auto result = reddish::parse_command_into(input, command);
    if (result)
    {
        if (!command.bytes_consumed || command.bytes_consumed > size) std::abort();
        reddish::Database database(64, 2 * 1024 * 1024);
        std::string output;
        try
        {
            reddish::execute_into(command, database, {}, output);
        }
        catch (const reddish::Error &)
        {
        }
        catch (const std::bad_alloc &)
        {
        }
    }
    return 0;
}
