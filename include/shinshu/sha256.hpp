#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace shinshu {

[[nodiscard]] std::string sha256(std::span<std::byte const> bytes);

} // namespace shinshu
