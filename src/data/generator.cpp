#include "shinshu/corpus.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

namespace shinshu {
namespace {

using Json = nlohmann::json;
constexpr std::string_view version = "1.0.0";
constexpr std::array<std::size_t, 4> primary_sizes{4096U, 16384U, 65536U, 262144U};
constexpr std::uint64_t pseudorandom_seed = 0x5348494e53485531ULL;
constexpr std::uint64_t sparse_seed = 0x5350415253450001ULL;
constexpr std::uint64_t mixed_seed = 0x4d49584544000100ULL;

class SplitMix64 {
  public:
    explicit SplitMix64(std::uint64_t seed) : state_(seed) {}
    std::uint64_t next() noexcept {
        auto value = (state_ += 0x9e3779b97f4a7c15ULL);
        value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31U);
    }

  private:
    std::uint64_t state_;
};

void fill_random(std::vector<std::byte>& bytes, std::size_t start, std::size_t length,
                 std::uint64_t seed) {
    SplitMix64 random(seed);
    std::size_t offset = start;
    while (offset < start + length) {
        const auto word = random.next();
        for (unsigned int index = 0; index < 8U && offset < start + length; ++index) {
            bytes[offset++] =
                static_cast<std::byte>(static_cast<std::uint8_t>((word >> (index * 8U)) & 0xffU));
        }
    }
}

void fill_pattern(std::vector<std::byte>& bytes, std::size_t start, std::size_t length,
                  std::string_view pattern) {
    for (std::size_t index = 0; index < length; ++index) {
        bytes[start + index] =
            static_cast<std::byte>(static_cast<unsigned char>(pattern[index % pattern.size()]));
    }
}

std::uint64_t seed_for(DataProfile profile) {
    switch (profile) {
    case DataProfile::pseudorandom:
        return pseudorandom_seed;
    case DataProfile::sparse_random:
        return sparse_seed;
    case DataProfile::mixed_entropy:
        return mixed_seed;
    default:
        return 0U;
    }
}

std::string code_for(DataProfile profile) {
    switch (profile) {
    case DataProfile::zero:
        return "G01";
    case DataProfile::repeat_abc:
        return "G02";
    case DataProfile::sequential:
        return "G03";
    case DataProfile::pseudorandom:
        return "G04";
    case DataProfile::generated_text:
        return "G05";
    case DataProfile::alternating:
        return "G06";
    case DataProfile::sparse_random:
        return "G07";
    case DataProfile::mixed_entropy:
        return "G08";
    case DataProfile::edge_case:
        return "E";
    }
    return "?";
}

Json params_for(DataProfile profile, std::size_t size, std::uint64_t seed) {
    switch (profile) {
    case DataProfile::zero:
        return Json{{"byte", 0}};
    case DataProfile::repeat_abc:
        return Json{{"pattern", "abc\\n"}, {"period_bytes", 4}};
    case DataProfile::sequential:
        return Json{{"modulus", 256}, {"start_byte", 0}};
    case DataProfile::pseudorandom:
        return Json{{"algorithm", "SplitMix64-v1"},
                    {"byte_order", "least-significant-byte-first"},
                    {"seed", seed}};
    case DataProfile::generated_text:
        return Json{{"line_tokens", 8}, {"token_order", "fixed-vocabulary-cycle-v1"}};
    case DataProfile::alternating:
        return Json{{"values", Json::array({0, 255})}, {"start_byte", 0}};
    case DataProfile::sparse_random:
        return Json{{"algorithm", "SplitMix64-v1"},
                    {"nonzero_count", size / 64U},
                    {"position_formula", "floor(i*size/nonzero_count)"},
                    {"seed", seed}};
    case DataProfile::mixed_entropy:
        return Json{{"block_bytes", 1024},
                    {"low_entropy_pattern", "abc\\n"},
                    {"random_algorithm", "SplitMix64-v1"},
                    {"seed", seed}};
    case DataProfile::edge_case:
        return Json::object();
    }
    return Json::object();
}

Json properties_for(DataProfile profile, std::size_t size) {
    switch (profile) {
    case DataProfile::zero:
        return Json{{"all_zero", true}};
    case DataProfile::repeat_abc:
        return Json{{"period_bytes", 4}, {"starts_with", "abc\\n"}};
    case DataProfile::sequential:
        return Json{{"cycle_bytes", 256}, {"starts_at_zero", true}};
    case DataProfile::pseudorandom:
        return Json{{"controlled_test_data", true}, {"generator", "SplitMix64-v1"}};
    case DataProfile::generated_text:
        return Json{{"text_encoding", "ASCII"}, {"vocabulary_size", 24}};
    case DataProfile::alternating:
        return Json{{"period_bytes", 2}, {"values", Json::array({0, 255})}};
    case DataProfile::sparse_random:
        return Json{{"nonzero_count", size / 64U}, {"zero_background", true}};
    case DataProfile::mixed_entropy:
        return Json{{"block_bytes", 1024}, {"starts_with_low_entropy", true}};
    case DataProfile::edge_case:
        return Json{{"edge_case", true}};
    }
    return Json::object();
}

DatasetRecord make_record(std::string id, DataProfile profile, std::size_t size, std::string path,
                          std::uint64_t seed, Json params, Json properties) {
    return {std::move(id),
            profile,
            std::string(version),
            seed,
            static_cast<std::uint64_t>(size),
            std::move(path),
            {},
            params.dump(),
            properties.dump()};
}

} // namespace

std::string_view profile_name(DataProfile profile) noexcept {
    switch (profile) {
    case DataProfile::zero:
        return "zero";
    case DataProfile::repeat_abc:
        return "repeat_abc";
    case DataProfile::sequential:
        return "sequential";
    case DataProfile::pseudorandom:
        return "pseudorandom";
    case DataProfile::generated_text:
        return "generated_text";
    case DataProfile::alternating:
        return "alternating";
    case DataProfile::sparse_random:
        return "sparse_random";
    case DataProfile::mixed_entropy:
        return "mixed_entropy";
    case DataProfile::edge_case:
        return "edge_cases";
    }
    return "unknown";
}

std::optional<DataProfile> parse_profile(std::string_view name) noexcept {
    constexpr std::array profiles{DataProfile::zero,           DataProfile::repeat_abc,
                                  DataProfile::sequential,     DataProfile::pseudorandom,
                                  DataProfile::generated_text, DataProfile::alternating,
                                  DataProfile::sparse_random,  DataProfile::mixed_entropy};
    for (const auto profile : profiles) {
        if (profile_name(profile) == name)
            return profile;
    }
    return std::nullopt;
}

DatasetRecord profile_dataset_spec(DataProfile profile, std::size_t size) {
    if (profile == DataProfile::edge_case) {
        throw std::invalid_argument("edge-case datasets require fixed edge-case specifications");
    }
    const std::string name{profile_name(profile)};
    const auto seed = seed_for(profile);
    return make_record(code_for(profile) + "-" + name + "-" + std::to_string(size) + "-v1", profile,
                       size,
                       "data/generated/" + name + "/" + name + "_" + std::to_string(size) + ".bin",
                       seed, params_for(profile, size, seed), properties_for(profile, size));
}

std::vector<std::byte> generate_profile_bytes(DataProfile profile, std::size_t size,
                                              std::uint64_t seed) {
    std::vector<std::byte> bytes(size, std::byte{0});
    constexpr std::string_view abc = "abc\n";
    constexpr std::array<std::string_view, 24> words{
        "amber",  "birch",   "cinder", "delta",   "ember",  "fable",  "garden", "harbor",
        "island", "jasmine", "kernel", "lantern", "meadow", "nectar", "ocean",  "petal",
        "quartz", "ribbon",  "silver", "timber",  "umber",  "velvet", "willow", "zephyr"};

    switch (profile) {
    case DataProfile::zero:
        break;
    case DataProfile::edge_case:
        throw std::invalid_argument("edge-case bytes require a named edge-case dataset ID");
    case DataProfile::repeat_abc:
        fill_pattern(bytes, 0, size, abc);
        break;
    case DataProfile::sequential:
        for (std::size_t index = 0; index < size; ++index) {
            bytes[index] = static_cast<std::byte>(static_cast<std::uint8_t>(index & 0xffU));
        }
        break;
    case DataProfile::pseudorandom:
        fill_random(bytes, 0, size, seed);
        break;
    case DataProfile::generated_text: {
        std::size_t offset = 0;
        std::size_t token = 0;
        while (offset < size) {
            for (const char character : words[token % words.size()]) {
                if (offset >= size)
                    break;
                bytes[offset++] = static_cast<std::byte>(static_cast<unsigned char>(character));
            }
            if (offset < size) {
                bytes[offset++] = static_cast<std::byte>(token % 8U == 7U ? '\n' : ' ');
            }
            ++token;
        }
        break;
    }
    case DataProfile::alternating:
        for (std::size_t index = 0; index < size; ++index) {
            bytes[index] = index % 2U == 0U ? std::byte{0x00} : std::byte{0xff};
        }
        break;
    case DataProfile::sparse_random: {
        SplitMix64 random(seed);
        const auto count = size / 64U;
        for (std::size_t index = 0; index < count; ++index) {
            const auto position = index * size / count;
            bytes[position] =
                static_cast<std::byte>(static_cast<std::uint8_t>(1U + random.next() % 255U));
        }
        break;
    }
    case DataProfile::mixed_entropy:
        for (std::size_t start = 0, block = 0; start < size; start += 1024U, ++block) {
            const auto length = std::min(std::size_t{1024}, size - start);
            if (block % 2U == 0U) {
                fill_pattern(bytes, start, length, abc);
            } else {
                fill_random(bytes, start, length, seed ^ static_cast<std::uint64_t>(block));
            }
        }
        break;
    }
    return bytes;
}

std::vector<DatasetRecord> required_datasets() {
    std::vector<DatasetRecord> records;
    constexpr std::array profiles{DataProfile::zero,           DataProfile::repeat_abc,
                                  DataProfile::sequential,     DataProfile::pseudorandom,
                                  DataProfile::generated_text, DataProfile::alternating,
                                  DataProfile::sparse_random,  DataProfile::mixed_entropy};
    for (const auto profile : profiles) {
        for (const auto size : primary_sizes) {
            records.push_back(profile_dataset_spec(profile, size));
        }
    }

    records.push_back(make_record("E00-empty-v1", DataProfile::edge_case, 0,
                                  "data/generated/edge_cases/empty.bin", 0, Json::object(),
                                  Json{{"edge_case", "empty"}}));
    records.push_back(make_record("E01-single-byte-v1", DataProfile::edge_case, 1,
                                  "data/generated/edge_cases/single_byte.bin", 0,
                                  Json{{"byte", 165}}, Json{{"edge_case", "single_byte"}}));
    records.push_back(make_record("E02-all-byte-values-v1", DataProfile::edge_case, 256,
                                  "data/generated/edge_cases/all_byte_values.bin", 0,
                                  Json{{"byte_order", "ascending"}},
                                  Json{{"contains_each_byte_once", true}}));
    constexpr std::array<std::size_t, 6> edge_sizes{255U, 256U, 257U, 4095U, 4096U, 4097U};
    for (const auto size : edge_sizes) {
        records.push_back(
            make_record("E03-size-" + std::to_string(size) + "-v1", DataProfile::edge_case, size,
                        "data/generated/edge_cases/size_" + std::to_string(size) + ".bin", 0,
                        Json{{"pattern", "sequential-mod-256"}},
                        Json{{"edge_case", "boundary_length"}, {"length", size}}));
    }
    return records;
}

} // namespace shinshu
