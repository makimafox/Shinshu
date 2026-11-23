#pragma once

#include <string_view>

namespace shinshu::cli {

enum class Action { help, version, compress, decompress, verify, benchmark, invalid };

struct ParsedCommand {
    Action action{Action::invalid};
    bool valid{false};
    std::string_view error{};
};

[[nodiscard]] ParsedCommand parse_arguments(int argc, char const* const* argv) noexcept;
[[nodiscard]] std::string_view action_name(Action action) noexcept;

} // namespace shinshu::cli
