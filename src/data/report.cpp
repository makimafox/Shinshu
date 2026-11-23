#include "shinshu/report.hpp"

#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace shinshu {
namespace {

using Json = nlohmann::json;

template <typename Value> Json optional_json(std::optional<Value> const& value) {
    return value ? Json(*value) : Json(nullptr);
}

Json to_json(RoundTripRecord const& record) {
    return Json{{"run_id", record.run_id},
                {"git_commit", record.git_commit},
                {"build_configuration", record.build_configuration},
                {"compiler", record.compiler},
                {"os", record.operating_system},
                {"architecture", record.architecture},
                {"dataset_id", record.dataset_id},
                {"generator_profile", record.generator_profile},
                {"input_bytes", record.input_bytes},
                {"input_sha256", record.input_sha256},
                {"codec", record.codec_name},
                {"codec_version", record.codec_version},
                {"measurement_class", record.infrastructure_only ? "INFRASTRUCTURE_ONLY" : "CODEC"},
                {"comparison_eligible", record.comparison_eligible},
                {"compressed_bytes", optional_json(record.compressed_bytes)},
                {"compression_ratio", optional_json(record.compression_ratio)},
                {"bits_per_byte", optional_json(record.bits_per_byte)},
                {"compression_microseconds", optional_json(record.compression_microseconds)},
                {"decompression_microseconds", optional_json(record.decompression_microseconds)},
                {"peak_memory_bytes", optional_json(record.peak_memory_bytes)},
                {"byte_mismatch_count", optional_json(record.mismatch_count)},
                {"first_mismatch_offset", optional_json(record.first_mismatch_offset)},
                {"roundtrip_valid", record.roundtrip_valid},
                {"failure_reason",
                 record.failure_reason.empty() ? Json(nullptr) : Json(record.failure_reason)}};
}

template <typename Value> std::optional<Value> read_optional(Json const& value, char const* key) {
    return value.at(key).is_null() ? std::nullopt
                                   : std::optional<Value>{value.at(key).get<Value>()};
}

RoundTripRecord from_json(Json const& value) {
    RoundTripRecord record;
    record.run_id = value.at("run_id").get<std::string>();
    record.git_commit = value.at("git_commit").get<std::string>();
    record.build_configuration = value.at("build_configuration").get<std::string>();
    record.compiler = value.at("compiler").get<std::string>();
    record.operating_system = value.at("os").get<std::string>();
    record.architecture = value.at("architecture").get<std::string>();
    record.dataset_id = value.at("dataset_id").get<std::string>();
    record.generator_profile = value.at("generator_profile").get<std::string>();
    record.input_bytes = value.at("input_bytes").get<std::uint64_t>();
    record.input_sha256 = value.at("input_sha256").get<std::string>();
    record.codec_name = value.at("codec").get<std::string>();
    record.codec_version = value.at("codec_version").get<std::string>();
    record.infrastructure_only = value.at("measurement_class") == "INFRASTRUCTURE_ONLY";
    record.comparison_eligible = value.at("comparison_eligible").get<bool>();
    record.compressed_bytes = read_optional<std::uint64_t>(value, "compressed_bytes");
    record.compression_ratio = read_optional<double>(value, "compression_ratio");
    record.bits_per_byte = read_optional<double>(value, "bits_per_byte");
    record.compression_microseconds =
        read_optional<std::uint64_t>(value, "compression_microseconds");
    record.decompression_microseconds =
        read_optional<std::uint64_t>(value, "decompression_microseconds");
    record.peak_memory_bytes = read_optional<std::uint64_t>(value, "peak_memory_bytes");
    record.mismatch_count = read_optional<std::uint64_t>(value, "byte_mismatch_count");
    record.first_mismatch_offset = read_optional<std::int64_t>(value, "first_mismatch_offset");
    record.roundtrip_valid = value.at("roundtrip_valid").get<bool>();
    if (!value.at("failure_reason").is_null()) {
        record.failure_reason = value.at("failure_reason").get<std::string>();
    }
    return record;
}

std::string csv_escape(std::string const& value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos)
        return value;
    std::string result{"\""};
    for (const char character : value) {
        if (character == '"')
            result += "\"\"";
        else
            result.push_back(character);
    }
    result.push_back('"');
    return result;
}

template <typename Value> std::string csv_optional(std::optional<Value> const& value) {
    if (!value)
        return {};
    if constexpr (std::is_floating_point_v<Value>) {
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<Value>::max_digits10) << *value;
        return output.str();
    }
    return std::to_string(*value);
}

void write_text(std::filesystem::path const& path, std::string const& text) {
    std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        output.close();
        if (!output)
            throw std::runtime_error("could not write report: " + path.string());
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        if (std::filesystem::exists(path)) {
            std::filesystem::remove(path);
            std::filesystem::rename(temporary, path);
        } else {
            std::filesystem::remove(temporary);
            throw std::filesystem::filesystem_error("could not publish report", path, error);
        }
    }
}

} // namespace

void write_validation_report(std::filesystem::path const& root, ValidationResult const& result) {
    Json gates = Json::array();
    for (const auto& gate : result.gates) {
        gates.push_back(
            Json{{"gate", gate.gate}, {"status", gate.status}, {"explanation", gate.explanation}});
    }
    const Json report{{"report_type", "validation"},
                      {"schema_version", 1},
                      {"result", result.passed ? "PASS" : "FAIL"},
                      {"dataset_count", result.records.size()},
                      {"gates", std::move(gates)}};
    write_text(root / "data/manifests/validation_report.json", report.dump(2) + "\n");
}

void write_roundtrip_records(std::filesystem::path const& root,
                             std::vector<RoundTripRecord> const& records) {
    Json entries = Json::array();
    for (const auto& record : records)
        entries.push_back(to_json(record));
    const Json report{{"report_type", "roundtrip_records"},
                      {"schema_version", 1},
                      {"records", std::move(entries)}};
    write_text(root / "data/manifests/roundtrip_records.json", report.dump(2) + "\n");
}

std::vector<RoundTripRecord> load_roundtrip_records(std::filesystem::path const& root) {
    std::ifstream input(root / "data/manifests/roundtrip_records.json", std::ios::binary);
    if (!input)
        throw std::runtime_error("no round-trip records found; run `shinshu test roundtrip` first");
    const auto document = Json::parse(input);
    if (document.value("report_type", std::string{}) != "roundtrip_records" ||
        document.value("schema_version", 0) != 1 || !document.contains("records") ||
        !document.at("records").is_array()) {
        throw std::runtime_error("round-trip records have an unsupported schema");
    }
    std::vector<RoundTripRecord> records;
    for (const auto& item : document.at("records"))
        records.push_back(from_json(item));
    return records;
}

void write_roundtrip_reports(std::filesystem::path const& root,
                             std::vector<RoundTripRecord> const& records) {
    Json entries = Json::array();
    for (const auto& record : records)
        entries.push_back(to_json(record));
    const Json report{{"report_type", "roundtrip_report"},
                      {"schema_version", 1},
                      {"records", std::move(entries)}};
    write_text(root / "data/manifests/roundtrip_report.json", report.dump(2) + "\n");

    std::string csv =
        "run_id,git_commit,build_configuration,compiler,os,architecture,dataset_id,"
        "generator_profile,input_bytes,input_sha256,codec,codec_version,measurement_class,"
        "comparison_eligible,compressed_bytes,compression_ratio,bits_per_byte,"
        "compression_microseconds,decompression_microseconds,peak_memory_bytes,"
        "byte_mismatch_count,first_mismatch_offset,roundtrip_valid,failure_reason\n";
    for (const auto& record : records) {
        csv += csv_escape(record.run_id) + "," + csv_escape(record.git_commit) + "," +
               csv_escape(record.build_configuration) + "," + csv_escape(record.compiler) + "," +
               csv_escape(record.operating_system) + "," + csv_escape(record.architecture) + "," +
               csv_escape(record.dataset_id) + "," + csv_escape(record.generator_profile) + "," +
               std::to_string(record.input_bytes) + "," + record.input_sha256 + "," +
               csv_escape(record.codec_name) + "," + csv_escape(record.codec_version) + "," +
               (record.infrastructure_only ? "INFRASTRUCTURE_ONLY" : "CODEC") + "," +
               (record.comparison_eligible ? "true" : "false") + "," +
               csv_optional(record.compressed_bytes) + "," +
               csv_optional(record.compression_ratio) + "," + csv_optional(record.bits_per_byte) +
               "," + csv_optional(record.compression_microseconds) + "," +
               csv_optional(record.decompression_microseconds) + "," +
               csv_optional(record.peak_memory_bytes) + "," + csv_optional(record.mismatch_count) +
               "," + csv_optional(record.first_mismatch_offset) + "," +
               (record.roundtrip_valid ? "true" : "false") + "," +
               csv_escape(record.failure_reason) + "\n";
    }
    write_text(root / "data/manifests/roundtrip_report.csv", csv);
}

} // namespace shinshu
