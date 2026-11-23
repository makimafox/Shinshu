#include "shinshu/roundtrip.hpp"

#include "shinshu/codec.hpp"
#include "shinshu/sha256.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

#ifndef SHINSHU_GIT_COMMIT
#define SHINSHU_GIT_COMMIT "unknown"
#endif
#ifndef SHINSHU_BUILD_CONFIG
#define SHINSHU_BUILD_CONFIG "unknown"
#endif
#ifndef SHINSHU_COMPILER_ID
#define SHINSHU_COMPILER_ID "unknown"
#endif

namespace shinshu {
namespace {

std::string operating_system() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "unknown";
#endif
}

std::string architecture() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

} // namespace

RoundTripRecord run_roundtrip(Corpus const& corpus, DatasetRecord const& dataset,
                              CodecAdapter const& codec, std::string run_id) {
    RoundTripRecord record;
    record.run_id = std::move(run_id);
    record.git_commit = SHINSHU_GIT_COMMIT;
    record.build_configuration = SHINSHU_BUILD_CONFIG;
    record.compiler = SHINSHU_COMPILER_ID;
    record.operating_system = operating_system();
    record.architecture = architecture();
    record.dataset_id = dataset.dataset_id;
    record.generator_profile = profile_name(dataset.profile);
    record.input_bytes = dataset.size_bytes;
    record.input_sha256 = dataset.sha256;
    record.codec_name = codec.name();
    record.codec_version = codec.version();
    record.infrastructure_only = codec.infrastructure_only();

    std::vector<std::byte> original;
    try {
        original = corpus.read_dataset(dataset);
    } catch (std::exception const& error) {
        record.failure_reason = error.what();
        return record;
    }
    if (original.size() != dataset.size_bytes || sha256(original) != dataset.sha256) {
        record.failure_reason = "input dataset failed size or SHA-256 verification";
        return record;
    }

    const auto compression_start = std::chrono::steady_clock::now();
    const auto encoded = codec.compress(original);
    const auto compression_end = std::chrono::steady_clock::now();
    record.compression_microseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(compression_end - compression_start)
            .count());
    if (encoded.status != CodecStatus::success) {
        if (encoded.status == CodecStatus::not_implemented) {
            record.compression_microseconds.reset();
        }
        record.failure_reason = encoded.error;
        return record;
    }

    record.compressed_bytes = static_cast<std::uint64_t>(encoded.bytes.size());
    if (!original.empty() && !encoded.bytes.empty()) {
        record.compression_ratio =
            static_cast<double>(original.size()) / static_cast<double>(encoded.bytes.size());
        record.bits_per_byte =
            static_cast<double>(encoded.bytes.size()) * 8.0 / static_cast<double>(original.size());
    }
    const auto decompression_start = std::chrono::steady_clock::now();
    const auto decoded = codec.decompress(encoded.bytes);
    const auto decompression_end = std::chrono::steady_clock::now();
    record.decompression_microseconds =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                       decompression_end - decompression_start)
                                       .count());
    if (decoded.status != CodecStatus::success) {
        record.failure_reason = decoded.error;
        return record;
    }

    const auto compare_length = std::max(original.size(), decoded.bytes.size());
    std::uint64_t mismatch_count = 0;
    for (std::size_t index = 0; index < compare_length; ++index) {
        const bool original_has_byte = index < original.size();
        const bool decoded_has_byte = index < decoded.bytes.size();
        if (!original_has_byte || !decoded_has_byte || original[index] != decoded.bytes[index]) {
            ++mismatch_count;
            if (!record.first_mismatch_offset) {
                record.first_mismatch_offset = static_cast<std::int64_t>(index);
            }
        }
    }
    record.mismatch_count = mismatch_count;
    record.roundtrip_valid = mismatch_count == 0 && sha256(decoded.bytes) == record.input_sha256;
    if (!record.roundtrip_valid) {
        record.failure_reason = "decoded bytes differ from original input";
    }
    record.comparison_eligible = record.roundtrip_valid && !record.infrastructure_only;
    return record;
}

RoundTripRecord run_roundtrip(Corpus const& corpus, DatasetRecord const& dataset,
                              std::string_view codec_name, std::string run_id) {
    const auto* codec = find_codec_adapter(codec_name);
    if (codec != nullptr) {
        return run_roundtrip(corpus, dataset, *codec, std::move(run_id));
    }
    RoundTripRecord record;
    record.run_id = std::move(run_id);
    record.dataset_id = dataset.dataset_id;
    record.generator_profile = profile_name(dataset.profile);
    record.input_bytes = dataset.size_bytes;
    record.input_sha256 = dataset.sha256;
    record.codec_name = codec_name;
    record.failure_reason = "unknown codec adapter";
    return record;
}

} // namespace shinshu
