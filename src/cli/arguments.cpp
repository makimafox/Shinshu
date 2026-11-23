#include "shinshu/cli.hpp"

#include <string_view>

namespace shinshu::cli {
namespace {

bool has_output_option(int argc, char const* const* argv) noexcept {
    for (int index = 2; index < argc; ++index) {
        if (argv[index] != nullptr && std::string_view{argv[index]} == "-o") {
            return index + 1 < argc && argv[index + 1] != nullptr && argv[index + 1][0] != '\0';
        }
    }
    return false;
}

} // namespace

ParsedCommand parse_arguments(int argc, char const* const* argv) noexcept {
    if (argc < 1 || argv == nullptr) {
        return {Action::invalid, false, "invalid argument vector"};
    }
    if (argc == 1) {
        return {Action::help, true, {}};
    }
    if (argv[1] == nullptr) {
        return {Action::invalid, false, "command is missing"};
    }

    const std::string_view command{argv[1]};
    if (command == "--help" || command == "-h") {
        return argc == 2
                   ? ParsedCommand{Action::help, true, {}}
                   : ParsedCommand{Action::invalid, false, "--help does not accept arguments"};
    }
    if (command == "--version" || command == "-V") {
        return argc == 2
                   ? ParsedCommand{Action::version, true, {}}
                   : ParsedCommand{Action::invalid, false, "--version does not accept arguments"};
    }
    if (command == "compress" || command == "decompress") {
        if (argc != 5 || argv[2] == nullptr || argv[2][0] == '\0' || argv[3] == nullptr ||
            !has_output_option(argc, argv)) {
            return {Action::invalid, false, "expected: shinshu <command> input -o output"};
        }
        return {command == "compress" ? Action::compress : Action::decompress, true, {}};
    }
    if (command == "verify" || command == "benchmark") {
        if (argc != 3 || argv[2] == nullptr || argv[2][0] == '\0') {
            return {Action::invalid, false, "expected: shinshu <command> input"};
        }
        return {command == "verify" ? Action::verify : Action::benchmark, true, {}};
    }
    return {Action::invalid, false, "unknown command or option"};
}

std::string_view action_name(Action action) noexcept {
    switch (action) {
    case Action::help:
        return "help";
    case Action::version:
        return "version";
    case Action::compress:
        return "compress";
    case Action::decompress:
        return "decompress";
    case Action::verify:
        return "verify";
    case Action::benchmark:
        return "benchmark";
    case Action::invalid:
        return "invalid";
    }
    return "invalid";
}

} // namespace shinshu::cli
