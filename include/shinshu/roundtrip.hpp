#pragma once

#include "shinshu/corpus.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace shinshu {

class CodecAdapter;

struct RoundTripRecord {
    std::string run_id;
    std::string git_commit;
    std::string build_configuration;
    std::string compiler;
    std::string operating_system;
    std::string architecture;
    std::string dataset_id;
    std::string generator_profile;
    std::uint64_t input_bytes{0};
    std::string input_sha256;
    std::string codec_name;
    std::string codec_version;
    bool infrastructure_only{false};
    bool comparison_eligible{false};
    std::optional<std::uint64_t> compressed_bytes;
    std::optional<double> compression_ratio;
    std::optional<double> bits_per_byte;
    std::optional<std::uint64_t> compression_microseconds;
    std::optional<std::uint64_t> decompression_microseconds;
    std::optional<std::uint64_t> peak_memory_bytes;
    std::optional<std::uint64_t> mismatch_count;
    std::optional<std::int64_t> first_mismatch_offset;
    bool roundtrip_valid{false};
    std::string failure_reason;
};

[[nodiscard]] RoundTripRecord run_roundtrip(Corpus const& corpus, DatasetRecord const& dataset,
                                            std::string_view codec_name, std::string run_id);
[[nodiscard]] RoundTripRecord run_roundtrip(Corpus const& corpus, DatasetRecord const& dataset,
                                            CodecAdapter const& codec, std::string run_id);

} // namespace shinshu
