#pragma once

#include <string>
#include <string_view>

namespace solace::environment
{
[[nodiscard]] std::string value(std::string_view key);
} // namespace solace::environment
