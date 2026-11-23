#include "shinshu/cli.hpp"
#include "shinshu/codec.hpp"
#include "shinshu/corpus.hpp"
#include "shinshu/format.hpp"
#include "shinshu/roundtrip.hpp"
#include "shinshu/sha256.hpp"
#include "shinshu/version.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        static std::atomic<unsigned int> counter{0};
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto base = std::filesystem::temp_directory_path();
        for (;;) {
            path_ = base / ("shinshu-test-" + std::to_string(tick) + "-" +
                            std::to_string(counter.fetch_add(1U)));
            if (std::filesystem::create_directory(path_))
                break;
        }
    }
    ~TemporaryDirectory() {
        std::filesystem::remove_all(path_);
    }
    std::filesystem::path const& path() const {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

std::string read_text(std::filesystem::path const& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void write_text(std::filesystem::path const& path, std::string const& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

} // namespace

TEST(Version, IsAvailable) {
    EXPECT_FALSE(shinshu::version().empty());
}

TEST(FormatContract, HeaderDefaultsAreExplicit) {
    const shinshu::FileHeader header;
    EXPECT_EQ(header.format_version, 1);
    EXPECT_EQ(header.original_size, 0);
    EXPECT_EQ(header.flags, 0);
}

TEST(CliArguments, ReportsVersionAction) {
    constexpr char const* argv[] = {"shinshu", "--version"};
    const auto parsed = shinshu::cli::parse_arguments(2, argv);
    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.action, shinshu::cli::Action::version);
}

TEST(Generator, SplitMix64HasStableGoldenVector) {
    const auto bytes = shinshu::generate_profile_bytes(shinshu::DataProfile::pseudorandom, 16, 0);
    constexpr std::array<unsigned int, 16> expected{0xaf, 0xcd, 0x1d, 0x7b, 0x39, 0xa8, 0x20, 0xe2,
                                                    0xf4, 0x65, 0xb9, 0xa1, 0x6a, 0x9e, 0x78, 0x6e};
    ASSERT_EQ(bytes.size(), expected.size());
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        EXPECT_EQ(std::to_integer<unsigned int>(bytes[index]), expected[index]);
    }
}

TEST(Generator, SeedAndSizeAreDeterministic) {
    const auto first =
        shinshu::generate_profile_bytes(shinshu::DataProfile::pseudorandom, 1024, 17);
    const auto repeated =
        shinshu::generate_profile_bytes(shinshu::DataProfile::pseudorandom, 1024, 17);
    const auto other =
        shinshu::generate_profile_bytes(shinshu::DataProfile::pseudorandom, 1024, 18);
    EXPECT_EQ(first, repeated);
    EXPECT_NE(first, other);
    EXPECT_EQ(first.size(), 1024);
}

TEST(Sha256, MatchesKnownVectors) {
    const std::vector<std::byte> empty;
    EXPECT_EQ(shinshu::sha256(empty),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    constexpr std::string_view abc = "abc";
    const auto bytes =
        std::span<std::byte const>{reinterpret_cast<std::byte const*>(abc.data()), abc.size()};
    EXPECT_EQ(shinshu::sha256(bytes),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Corpus, RequiredSetHas32PrimaryAndNineEdgeCases) {
    const auto records = shinshu::required_datasets();
    ASSERT_EQ(records.size(), 41);
    EXPECT_EQ(std::count_if(records.begin(), records.end(),
                            [](auto const& record) {
                                return record.profile != shinshu::DataProfile::edge_case;
                            }),
              32);
}

TEST(Corpus, GenerateValidateAndDetectTampering) {
    TemporaryDirectory temporary;
    shinshu::Corpus corpus(temporary.path());
    const auto generated = corpus.generate_all();
    ASSERT_EQ(generated.size(), 41);
    const auto result = corpus.validate();
    ASSERT_TRUE(result.passed);
    for (const auto& gate : result.gates)
        EXPECT_EQ(gate.status, "PASS") << gate.gate;

    const auto sample = std::find_if(generated.begin(), generated.end(), [](auto const& record) {
        return record.dataset_id == "G01-zero-4096-v1";
    });
    ASSERT_NE(sample, generated.end());
    const auto file = temporary.path() / sample->relative_path;
    std::vector<std::byte> tampered(static_cast<std::size_t>(sample->size_bytes), std::byte{0x7e});
    std::ofstream damaged(file, std::ios::binary | std::ios::trunc);
    damaged.write(reinterpret_cast<char const*>(tampered.data()),
                  static_cast<std::streamsize>(tampered.size()));
    damaged.close();
    const auto corrupted = corpus.validate();
    EXPECT_FALSE(corrupted.passed);
    EXPECT_NE(corrupted.gates[2].status, "PASS");
    EXPECT_NE(corrupted.gates[6].status, "PASS");
}

TEST(Corpus, ExistingFileMustMatchBeforeReuse) {
    TemporaryDirectory temporary;
    shinshu::Corpus corpus(temporary.path());
    const auto record = corpus.generate_one("pseudorandom", 65536);
    EXPECT_EQ(corpus.generate_one("pseudorandom", 65536).sha256, record.sha256);
    write_text(temporary.path() / record.relative_path, "modified");
    EXPECT_THROW(static_cast<void>(corpus.generate_one("pseudorandom", 65536)), std::runtime_error);
}

TEST(Corpus, RejectsDuplicateIdsUnsafePathsAndMalformedJson) {
    TemporaryDirectory temporary;
    shinshu::Corpus corpus(temporary.path());
    const auto generated = corpus.generate_one("zero", 128);
    EXPECT_FALSE(generated.sha256.empty());
    const auto manifest_path = temporary.path() / "data/manifests/corpus.json";
    auto document = nlohmann::json::parse(read_text(manifest_path));

    auto duplicate = document;
    duplicate["entries"].push_back(duplicate["entries"][0]);
    write_text(manifest_path, duplicate.dump(2));
    EXPECT_THROW(static_cast<void>(corpus.list()), std::runtime_error);

    document["entries"][0]["relative_path"] = "../../outside.bin";
    write_text(manifest_path, document.dump(2));
    EXPECT_THROW(static_cast<void>(corpus.list()), std::runtime_error);
    EXPECT_FALSE(shinshu::is_safe_relative_path("../../outside.bin"));
    EXPECT_FALSE(shinshu::is_safe_relative_path("C:\\outside.bin"));

    document["entries"][0]["relative_path"] = generated.relative_path;
    document["entries"][0]["expected_properties"] = nlohmann::json::object();
    write_text(manifest_path, document.dump(2));
    EXPECT_THROW(static_cast<void>(corpus.list()), std::runtime_error);

    write_text(manifest_path, "{not-json");
    EXPECT_THROW(static_cast<void>(corpus.list()), nlohmann::json::exception);
}

TEST(Corpus, MissingManifestReportsSkippedChecks) {
    TemporaryDirectory temporary;
    const shinshu::Corpus corpus(temporary.path());
    const auto result = corpus.validate();
    EXPECT_FALSE(result.passed);
    ASSERT_EQ(result.gates.size(), 8);
    EXPECT_EQ(result.gates[0].status, "SKIP");
    EXPECT_EQ(result.gates[5].status, "FAIL");
    EXPECT_EQ(result.gates[7].status, "FAIL");
}

TEST(Corpus, EmptyAndBoundaryFilesHaveExactBytes) {
    TemporaryDirectory temporary;
    shinshu::Corpus corpus(temporary.path());
    const auto records = corpus.generate_all();
    for (const auto& record : records) {
        const auto bytes = shinshu::read_binary_file(temporary.path() / record.relative_path);
        EXPECT_EQ(bytes.size(), record.size_bytes) << record.dataset_id;
        EXPECT_EQ(shinshu::sha256(bytes), record.sha256) << record.dataset_id;
    }
    const auto empty = std::find_if(records.begin(), records.end(), [](auto const& record) {
        return record.dataset_id == "E00-empty-v1";
    });
    ASSERT_NE(empty, records.end());
    EXPECT_TRUE(shinshu::read_binary_file(temporary.path() / empty->relative_path).empty());
}

TEST(RoundTrip, IdentitySucceedsButIsInfrastructureOnly) {
    TemporaryDirectory temporary;
    shinshu::Corpus corpus(temporary.path());
    const auto dataset = corpus.generate_one("sequential", 257);
    const auto result = shinshu::run_roundtrip(corpus, dataset, "identity", "test-run");
    EXPECT_TRUE(result.roundtrip_valid);
    EXPECT_EQ(result.mismatch_count, 0);
    EXPECT_EQ(result.compressed_bytes, dataset.size_bytes);
    EXPECT_TRUE(result.infrastructure_only);
    EXPECT_FALSE(result.comparison_eligible);
}

TEST(RoundTrip, EmptyInputIdentityIsExact) {
    TemporaryDirectory temporary;
    shinshu::Corpus corpus(temporary.path());
    const auto dataset = corpus.generate_one("zero", 0);
    const auto result = shinshu::run_roundtrip(corpus, dataset, "identity", "empty-run");
    EXPECT_TRUE(result.roundtrip_valid);
    EXPECT_EQ(result.mismatch_count, 0);
    EXPECT_FALSE(result.bits_per_byte.has_value());
}

TEST(RoundTrip, ReportsMismatchCountAndOffset) {
    class MismatchCodec final : public shinshu::CodecAdapter {
      public:
        std::string_view name() const noexcept override {
            return "test-mismatch";
        }
        std::string_view version() const noexcept override {
            return "test";
        }
        bool infrastructure_only() const noexcept override {
            return true;
        }
        shinshu::CodecResult compress(std::span<std::byte const> input) const override {
            return {shinshu::CodecStatus::success, {input.begin(), input.end()}, {}};
        }
        shinshu::CodecResult decompress(std::span<std::byte const> input) const override {
            std::vector<std::byte> output(input.begin(), input.end());
            output[3] = std::byte{0xff};
            return {shinshu::CodecStatus::success, std::move(output), {}};
        }
    } codec;

    TemporaryDirectory temporary;
    shinshu::Corpus corpus(temporary.path());
    const auto dataset = corpus.generate_one("zero", 8);
    const auto result = shinshu::run_roundtrip(corpus, dataset, codec, "mismatch-run");
    EXPECT_FALSE(result.roundtrip_valid);
    EXPECT_EQ(result.mismatch_count, 1);
    EXPECT_EQ(result.first_mismatch_offset, 3);
}

TEST(RoundTrip, UnimplementedBackendCannotSucceedOrReportCompression) {
    TemporaryDirectory temporary;
    shinshu::Corpus corpus(temporary.path());
    const auto dataset = corpus.generate_one("zero", 32);
    const auto result = shinshu::run_roundtrip(corpus, dataset, "gzip", "test-run");
    EXPECT_FALSE(result.roundtrip_valid);
    EXPECT_FALSE(result.compressed_bytes.has_value());
    EXPECT_NE(result.failure_reason.find("not implemented"), std::string::npos);
}

TEST(Codec, UnknownNameDoesNotFallBack) {
    EXPECT_EQ(shinshu::find_codec_adapter("unknown"), nullptr);
    const auto result = shinshu::unimplemented_codec("gzip").compress({});
    EXPECT_EQ(result.status, shinshu::CodecStatus::not_implemented);
}
