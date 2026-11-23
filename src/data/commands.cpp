#include "shinshu/commands.hpp"

#include "shinshu/codec.hpp"
#include "shinshu/corpus.hpp"
#include "shinshu/report.hpp"
#include "shinshu/roundtrip.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace shinshu::cli {
namespace {

void print_data_help() {
    std::cout << "Usage:\n"
              << "  shinshu data generate --all\n"
              << "  shinshu data generate --profile <name> --size <bytes>\n"
              << "  shinshu data validate\n"
              << "  shinshu data list\n"
              << "  shinshu data summary\n";
}

void print_test_help() {
    std::cout << "Usage:\n"
              << "  shinshu test roundtrip --codec <name>\n"
              << "  shinshu test report\n"
              << "Codecs: identity, gzip, xz, zstd, neural, neural-residual, neural-probability\n";
}

std::size_t parse_size(char const* text) {
    if (text == nullptr || *text == '\0') {
        throw std::invalid_argument("size must be an unsigned integer");
    }
    const std::string_view input{text};
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);
    if (error != std::errc{} || end != input.data() + input.size() ||
        value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::invalid_argument(
            "size must be an unsigned integer within this platform's limit");
    }
    return static_cast<std::size_t>(value);
}

std::string create_run_id() {
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    return "run-" + std::to_string(now);
}

int invalid_arguments(std::string_view message) {
    std::cerr << "shinshu: " << message << '\n';
    return 2;
}

int run_generate(Corpus& corpus, int argc, char const* const* argv) {
    if (argc == 4 && argv[3] != nullptr && std::string_view{argv[3]} == "--all") {
        const auto records = corpus.generate_all();
        std::cout << "Generated or verified " << records.size() << " corpus files.\n";
        return 0;
    }
    if (argc != 7 || argv[3] == nullptr || std::string_view{argv[3]} != "--profile" ||
        argv[4] == nullptr || argv[5] == nullptr || std::string_view{argv[5]} != "--size") {
        return invalid_arguments(
            "expected: shinshu data generate --all OR --profile <name> --size <bytes>");
    }
    const auto record = corpus.generate_one(argv[4], parse_size(argv[6]));
    std::cout << "Generated or verified " << record.dataset_id << " (" << record.size_bytes
              << " bytes, sha256 " << record.sha256 << ").\n";
    return 0;
}

int run_validate(Corpus const& corpus) {
    const auto result = corpus.validate();
    write_validation_report(corpus.root(), result);
    for (const auto& gate : result.gates) {
        std::cout << gate.gate << ' ' << gate.status << " - " << gate.explanation << '\n';
    }
    std::cout << "Validation report: data/manifests/validation_report.json\n";
    return result.passed ? 0 : 1;
}

int run_list(Corpus const& corpus) {
    for (const auto& record : corpus.list()) {
        std::cout << record.dataset_id << '\t' << profile_name(record.profile) << '\t'
                  << record.size_bytes << '\t' << record.sha256 << '\t' << record.relative_path
                  << '\n';
    }
    return 0;
}

int run_summary(Corpus const& corpus) {
    const auto records = corpus.list();
    std::map<std::string, std::pair<std::size_t, std::uint64_t>> summary;
    for (const auto& record : records) {
        auto& item = summary[std::string(profile_name(record.profile))];
        ++item.first;
        item.second += record.size_bytes;
    }
    std::cout << "Datasets: " << records.size() << '\n';
    std::uint64_t total_bytes = 0;
    for (const auto& [profile, values] : summary) {
        std::cout << profile << ": " << values.first << " files, " << values.second << " bytes\n";
        total_bytes += values.second;
    }
    std::cout << "Total input bytes: " << total_bytes << '\n';
    return 0;
}

} // namespace

int run_data_command(int argc, char const* const* argv) {
    if (argc == 3 && argv[2] != nullptr && std::string_view{argv[2]} == "--help") {
        print_data_help();
        return 0;
    }
    if (argc < 3 || argv[2] == nullptr) {
        return invalid_arguments("expected a data subcommand; use `shinshu data --help`");
    }
    Corpus corpus(std::filesystem::current_path());
    const std::string_view command{argv[2]};
    if (command == "generate") {
        return run_generate(corpus, argc, argv);
    }
    if (command == "validate" && argc == 3) {
        return run_validate(corpus);
    }
    if (command == "list" && argc == 3) {
        return run_list(corpus);
    }
    if (command == "summary" && argc == 3) {
        return run_summary(corpus);
    }
    return invalid_arguments("unknown data command or unexpected arguments");
}

int run_test_command(int argc, char const* const* argv) {
    if (argc == 3 && argv[2] != nullptr && std::string_view{argv[2]} == "--help") {
        print_test_help();
        return 0;
    }
    const auto root = std::filesystem::current_path();
    if (argc == 3 && argv[2] != nullptr && std::string_view{argv[2]} == "report") {
        const auto records = load_roundtrip_records(root);
        write_roundtrip_reports(root, records);
        std::cout << "Reports written to data/manifests/roundtrip_report.json and .csv\n";
        return 0;
    }
    if (argc != 5 || argv[2] == nullptr || std::string_view{argv[2]} != "roundtrip" ||
        argv[3] == nullptr || std::string_view{argv[3]} != "--codec" || argv[4] == nullptr) {
        return invalid_arguments(
            "expected: shinshu test roundtrip --codec <name> OR shinshu test report");
    }
    const std::string_view codec_name{argv[4]};
    const auto* adapter = find_codec_adapter(codec_name);
    if (adapter == nullptr) {
        return invalid_arguments("unknown codec adapter; use `shinshu test --help`");
    }

    Corpus corpus(root);
    const auto datasets = corpus.list();
    if (datasets.empty()) {
        std::cerr << "shinshu: corpus is empty; run `shinshu data generate --all` first\n";
        return 1;
    }
    const auto run_id = create_run_id();
    std::vector<RoundTripRecord> records;
    records.reserve(datasets.size());
    bool success = true;
    for (const auto& dataset : datasets) {
        auto record = run_roundtrip(corpus, dataset, codec_name, run_id);
        success = success && record.roundtrip_valid;
        if (!record.roundtrip_valid) {
            std::cerr << "FAIL " << record.dataset_id << ": " << record.failure_reason << '\n';
        } else {
            std::cout << "PASS " << record.dataset_id << " (" << record.input_bytes << " bytes)\n";
        }
        records.push_back(std::move(record));
    }
    write_roundtrip_records(root, records);
    write_roundtrip_reports(root, records);
    if (adapter->infrastructure_only()) {
        std::cout
            << "INFRASTRUCTURE_ONLY: identity measurements are excluded from codec comparisons.\n";
    }
    std::cout << "Reports written under data/manifests/roundtrip_report.*\n";
    return success ? 0 : (adapter->version() == "unavailable" ? 3 : 1);
}

} // namespace shinshu::cli
