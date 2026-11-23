#include "shinshu/cli.hpp"
#include "shinshu/version.hpp"

#include <iostream>

namespace {

void print_help() {
    std::cout
        << "Neural Memorization Compression " << shinshu::version() << "\n"
        << "Usage:\n"
        << "  shinshu --help\n"
        << "  shinshu --version\n"
        << "  shinshu compress input.bin -o output.shinshu\n"
        << "  shinshu decompress input.shinshu -o output.bin\n"
        << "  shinshu verify input.shinshu\n"
        << "  shinshu benchmark input.bin\n"
        << "Compression, decompression, verification, and benchmarking are not implemented yet.\n";
}

} // namespace

int main(int argc, char const* const* argv) {
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
