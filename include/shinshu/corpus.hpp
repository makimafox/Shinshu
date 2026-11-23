#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shinshu {

enum class DataProfile {
    zero,
    repeat_abc,
    sequential,
    pseudorandom,
    generated_text,
    alternating,
    sparse_random,
    mixed_entropy,
    edge_case
};

struct DatasetRecord {
    std::string dataset_id;
    DataProfile profile{DataProfile::zero};
    std::string generator_version;
    std::uint64_t seed{0};
    std::uint64_t size_bytes{0};
    std::string relative_path;
    std::string sha256;
    std::string parameters_json;
    std::string properties_json;
};

struct GateResult {
    std::string gate;
    std::string status;
    std::string explanation;
};

struct ValidationResult {
    bool passed{false};
    std::vector<GateResult> gates;
    std::vector<DatasetRecord> records;
};

[[nodiscard]] std::string_view profile_name(DataProfile profile) noexcept;
[[nodiscard]] std::optional<DataProfile> parse_profile(std::string_view name) noexcept;
[[nodiscard]] std::vector<std::byte> generate_profile_bytes(DataProfile profile, std::size_t size,
                                                            std::uint64_t seed);
[[nodiscard]] DatasetRecord profile_dataset_spec(DataProfile profile, std::size_t size);
[[nodiscard]] std::vector<DatasetRecord> required_datasets();

class Corpus {
  public:
    explicit Corpus(std::filesystem::path root);

    [[nodiscard]] std::vector<DatasetRecord> generate_all();
    [[nodiscard]] DatasetRecord generate_one(std::string_view profile, std::size_t size);
    [[nodiscard]] std::vector<DatasetRecord> list() const;
    [[nodiscard]] std::vector<std::byte> read_dataset(DatasetRecord const& record) const;
    [[nodiscard]] ValidationResult validate() const;
    [[nodiscard]] std::filesystem::path const& root() const noexcept;

  private:
    std::filesystem::path root_;
};

[[nodiscard]] bool is_safe_relative_path(std::string_view path) noexcept;
[[nodiscard]] std::vector<std::byte> read_binary_file(std::filesystem::path const& path);

} // namespace shinshu
