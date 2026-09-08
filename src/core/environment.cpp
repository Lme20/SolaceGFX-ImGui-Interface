#include "core/environment.h"

#include "core/product_info.h"

#include <cstdlib>
#include <string>

namespace solace::environment
{
namespace
{
std::string read_variable(const std::string& name)
{
#if defined(_MSC_VER)

    char* buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, name.c_str()) != 0 || !buffer)
        return {};
    std::string result(buffer);
    std::free(buffer);
    return result;
#else
    const char* raw = std::getenv(name.c_str());
    return raw ? std::string(raw) : std::string{};
#endif
}
} // namespace

std::string value(std::string_view key)
{
    return read_variable(product_info::environment_prefix + std::string(key));
}
} // namespace solace::environment
