#include "shinshu/cli.hpp"
#include "shinshu/commands.hpp"
#include "shinshu/version.hpp"

#include <exception>
#include <iostream>
#include <string_view>

namespace {

void print_help() {
    std::cout << "Neural Memorization Compression " << shinshu::version() << "\n"
              << "Usage:\n"
              << "  shinshu --help\n"
              << "  shinshu --version\n"
              << "  shinshu compress input.bin -o output.shinshu\n"
              << "  shinshu decompress input.shinshu -o output.bin\n"
              << "  shinshu verify input.shinshu\n"
              << "  shinshu benchmark input.bin\n"
              << "  shinshu data generate --all\n"
              << "  shinshu data validate|list|summary\n"
              << "  shinshu test roundtrip --codec identity\n"
              << "  shinshu test report\n"
              << "Codec backends other than identity are not implemented yet.\n";
}

} // namespace

int main(int argc, char const* const* argv) {
    if (argc >= 2 && argv[1] != nullptr && std::string_view{argv[1]} == "data") {
        try {
            return shinshu::cli::run_data_command(argc, argv);
        } catch (std::invalid_argument const& error) {
            std::cerr << "shinshu: " << error.what() << '\n';
            return 2;
        } catch (std::exception const& error) {
            std::cerr << "shinshu: " << error.what() << '\n';
            return 1;
        }
    }
    if (argc >= 2 && argv[1] != nullptr && std::string_view{argv[1]} == "test") {
        try {
            return shinshu::cli::run_test_command(argc, argv);
        } catch (std::invalid_argument const& error) {
            std::cerr << "shinshu: " << error.what() << '\n';
            return 2;
        } catch (std::exception const& error) {
            std::cerr << "shinshu: " << error.what() << '\n';
            return 1;
        }
    }
    const auto parsed = shinshu::cli::parse_arguments(argc, argv);
    if (!parsed.valid) {
        std::cerr << "shinshu: " << parsed.error << "\n";
        return 2;
    }
    if (parsed.action == shinshu::cli::Action::help) {
        print_help();
        return 0;
    }
    if (parsed.action == shinshu::cli::Action::version) {
        std::cout << shinshu::version() << '\n';
        return 0;
    }
    std::cerr << "shinshu: " << shinshu::cli::action_name(parsed.action) << " is not implemented\n";
    return 1;
}
