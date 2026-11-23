#include "shinshu/corpus.hpp"

#include "shinshu/sha256.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>

namespace shinshu {
namespace {

using Json = nlohmann::json;
constexpr std::string_view generator_version = "1.0.0";
constexpr std::size_t maximum_generated_size = 256U * 1024U * 1024U;
std::atomic<std::uint64_t> temporary_counter{0};

std::vector<std::byte> regenerate(DatasetRecord const& record) {
    if (record.profile != DataProfile::edge_case) {
        return generate_profile_bytes(record.profile, static_cast<std::size_t>(record.size_bytes),
                                      record.seed);
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(record.size_bytes));
    if (record.dataset_id == "E01-single-byte-v1") {
        if (!bytes.empty()) {
            bytes[0] = std::byte{0xa5};
        }
    } else if (record.dataset_id == "E02-all-byte-values-v1") {
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            bytes[index] = static_cast<std::byte>(static_cast<std::uint8_t>(index));
        }
    } else if (record.dataset_id != "E00-empty-v1") {
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            bytes[index] = static_cast<std::byte>(static_cast<std::uint8_t>(index & 0xffU));
        }
    }
    return bytes;
}

Json record_to_json(DatasetRecord const& record) {
    return Json{{"dataset_id", record.dataset_id},
                {"profile", profile_name(record.profile)},
                {"generator_version", record.generator_version},
                {"seed", record.seed},
                {"size_bytes", record.size_bytes},
                {"relative_path", record.relative_path},
                {"sha256", record.sha256},
                {"parameters", Json::parse(record.parameters_json)},
                {"expected_properties", Json::parse(record.properties_json)}};
}

std::string checksum_string(Json const& value, char const* key) {
    if (!value.contains(key) || !value.at(key).is_string()) {
        throw std::runtime_error(std::string("manifest field is missing or has wrong type: ") +
                                 key);
    }
    const auto checksum = value.at(key).get<std::string>();
    if (checksum.size() != 64U ||
        !std::all_of(checksum.begin(), checksum.end(), [](char character) {
            return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
        })) {
        throw std::runtime_error("manifest contains an invalid SHA-256 value");
    }
    return checksum;
}

DatasetRecord record_from_json(Json const& value) {
    if (!value.is_object()) {
        throw std::runtime_error("manifest entry must be an object");
    }
    const auto profile_text = value.at("profile").get<std::string>();
    DataProfile profile = DataProfile::edge_case;
    if (profile_text != "edge_cases") {
        const auto parsed = parse_profile(profile_text);
        if (!parsed) {
            throw std::runtime_error("manifest entry has an unknown profile");
        }
        profile = *parsed;
    }
    const auto parameters = value.at("parameters");
    const auto properties = value.at("expected_properties");
    if (!parameters.is_object() || !properties.is_object()) {
        throw std::runtime_error("manifest parameters and expected_properties must be objects");
    }
    const auto entry_version = value.at("generator_version").get<std::string>();
    const auto size_bytes = value.at("size_bytes").get<std::uint64_t>();
    if (entry_version != generator_version || size_bytes > maximum_generated_size) {
        throw std::runtime_error("manifest entry has an unsupported version or size");
    }
    return {value.at("dataset_id").get<std::string>(),
            profile,
            entry_version,
            value.at("seed").get<std::uint64_t>(),
            size_bytes,
            value.at("relative_path").get<std::string>(),
            checksum_string(value, "sha256"),
            parameters.dump(),
            properties.dump()};
}

std::vector<DatasetRecord> parse_manifest(std::string const& contents) {
    const auto document = Json::parse(contents);
    if (!document.is_object() || document.value("schema_version", 0) != 1 ||
        document.value("generator_version", std::string{}) != generator_version ||
        !document.contains("entries") || !document.at("entries").is_array()) {
        throw std::runtime_error("manifest does not match schema version 1");
    }
    std::vector<DatasetRecord> records;
    std::set<std::string> ids;
    std::set<std::string> paths;
    for (const auto& item : document.at("entries")) {
        auto record = record_from_json(item);
        if (record.dataset_id.empty() || !is_safe_relative_path(record.relative_path)) {
            throw std::runtime_error("manifest contains an empty ID or unsafe relative path");
        }
        if (!ids.insert(record.dataset_id).second) {
            throw std::runtime_error("manifest contains duplicate dataset IDs");
        }
        if (!paths.insert(record.relative_path).second) {
            throw std::runtime_error("manifest contains duplicate dataset paths");
        }
        records.push_back(std::move(record));
    }
    if (!std::is_sorted(records.begin(), records.end(), [](auto const& left, auto const& right) {
            return left.dataset_id < right.dataset_id;
        })) {
        throw std::runtime_error("manifest entries are not sorted by dataset_id");
    }
    const auto required = required_datasets();
    for (const auto& record : records) {
        DatasetRecord expected;
        if (record.profile == DataProfile::edge_case) {
            const auto found =
                std::find_if(required.begin(), required.end(), [&](auto const& item) {
                    return item.dataset_id == record.dataset_id;
                });
            if (found == required.end()) {
                throw std::runtime_error("manifest contains an unknown edge-case ID");
            }
            expected = *found;
        } else {
            expected =
                profile_dataset_spec(record.profile, static_cast<std::size_t>(record.size_bytes));
        }
        if (record.dataset_id != expected.dataset_id || record.profile != expected.profile ||
            record.generator_version != expected.generator_version ||
            record.seed != expected.seed || record.size_bytes != expected.size_bytes ||
            record.relative_path != expected.relative_path ||
            record.parameters_json != expected.parameters_json ||
            record.properties_json != expected.properties_json) {
            throw std::runtime_error(
                "manifest metadata is inconsistent with deterministic generator rules");
        }
    }
    return records;
}

std::string serialize_manifest(std::vector<DatasetRecord> records) {
    std::sort(records.begin(), records.end(), [](auto const& left, auto const& right) {
        return left.dataset_id < right.dataset_id;
    });
    Json entries = Json::array();
    for (const auto& record : records) {
        entries.push_back(record_to_json(record));
    }
    return Json{{"schema_version", 1},
                {"generator_version", generator_version},
                {"entries", std::move(entries)}}
               .dump(2) +
           "\n";
}

std::filesystem::path resolved_path(std::filesystem::path const& root,
                                    std::string_view relative_path) {
    if (!is_safe_relative_path(relative_path)) {
        throw std::runtime_error("unsafe relative path");
    }
    const auto canonical_root = std::filesystem::weakly_canonical(root);
    auto current = canonical_root;
    for (const auto& component : std::filesystem::path(relative_path)) {
        current /= component;
        std::error_code status_error;
        const auto status = std::filesystem::symlink_status(current, status_error);
        if (!status_error && std::filesystem::is_symlink(status)) {
            throw std::runtime_error("dataset path contains a symbolic link");
        }
        if (status_error && status_error != std::errc::no_such_file_or_directory) {
            throw std::filesystem::filesystem_error("cannot inspect dataset path", current,
                                                    status_error);
        }
    }
    const auto candidate =
        std::filesystem::weakly_canonical(canonical_root / std::filesystem::path(relative_path));
    const auto relative = candidate.lexically_relative(canonical_root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
        throw std::runtime_error("resolved path escapes corpus root");
    }
    return candidate;
}

std::filesystem::path manifest_path(std::filesystem::path const& root) {
    return resolved_path(root, "data/manifests/corpus.json");
}

void write_immutable_file(std::filesystem::path const& path,
                          std::span<std::byte const> expected_bytes,
                          std::string_view expected_hash) {
    if (std::filesystem::exists(path)) {
        const auto existing = read_binary_file(path);
        if (sha256(existing) != expected_hash) {
            throw std::runtime_error("refusing to reuse modified generated file: " + path.string());
        }
        return;
    }

    std::filesystem::create_directories(path.parent_path());
    const auto temporary =
        path.string() + ".tmp." + std::to_string(temporary_counter.fetch_add(1U));
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("cannot create temporary dataset file");
        }
        if (!expected_bytes.empty()) {
            output.write(reinterpret_cast<char const*>(expected_bytes.data()),
                         static_cast<std::streamsize>(expected_bytes.size()));
        }
        output.close();
        if (!output) {
            throw std::runtime_error("failed while writing dataset file");
        }
        std::error_code link_error;
        std::filesystem::create_hard_link(temporary, path, link_error);
        if (link_error) {
            if (std::filesystem::exists(path)) {
                const auto existing = read_binary_file(path);
                if (sha256(existing) != expected_hash) {
                    throw std::runtime_error("refusing to reuse modified generated file: " +
                                             path.string());
                }
            } else {
                std::error_code copy_error;
                std::filesystem::copy_file(temporary, path, std::filesystem::copy_options::none,
                                           copy_error);
                if (copy_error) {
                    if (std::filesystem::exists(path)) {
                        const auto existing = read_binary_file(path);
                        if (sha256(existing) != expected_hash) {
                            throw std::runtime_error("refusing to reuse modified generated file: " +
                                                     path.string());
                        }
                    } else {
                        throw std::filesystem::filesystem_error("cannot publish immutable dataset",
                                                                path, copy_error);
                    }
                }
            }
        }
        std::filesystem::remove(temporary);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

void write_manifest(std::filesystem::path const& path, std::vector<DatasetRecord> records) {
    std::filesystem::create_directories(path.parent_path());
    const auto temporary =
        path.string() + ".tmp." + std::to_string(temporary_counter.fetch_add(1U));
    try {
        const auto contents = serialize_manifest(std::move(records));
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.close();
        if (!output) {
            throw std::runtime_error("failed while writing corpus manifest");
        }
        std::error_code rename_error;
        std::filesystem::rename(temporary, path, rename_error);
        if (rename_error) {
            // Windows rename does not replace an existing destination.
            if (std::filesystem::exists(path)) {
                std::filesystem::remove(path);
                std::filesystem::rename(temporary, path);
            } else {
                throw std::filesystem::filesystem_error("cannot publish corpus manifest", path,
                                                        rename_error);
            }
        }
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

std::vector<DatasetRecord> load_existing_manifest(std::filesystem::path const& root) {
    const auto path = manifest_path(root);
    if (!std::filesystem::exists(path)) {
        return {};
    }
    const auto contents = read_binary_file(path);
    const std::string text(reinterpret_cast<char const*>(contents.data()), contents.size());
    auto records = parse_manifest(text);
    for (const auto& record : records) {
        const auto dataset_path = resolved_path(root, record.relative_path);
        if (std::filesystem::file_size(dataset_path) != record.size_bytes) {
            throw std::runtime_error("existing manifest dataset has wrong size: " +
                                     record.dataset_id);
        }
        const auto bytes = read_binary_file(dataset_path);
        if (bytes.size() != record.size_bytes || sha256(bytes) != record.sha256) {
            throw std::runtime_error("existing manifest dataset failed integrity check: " +
                                     record.dataset_id);
        }
    }
    return records;
}

void merge_record(std::vector<DatasetRecord>& records, DatasetRecord record) {
    const auto found = std::find_if(records.begin(), records.end(), [&](auto const& existing) {
        return existing.dataset_id == record.dataset_id;
    });
    if (found == records.end()) {
        records.push_back(std::move(record));
    } else if (found->sha256 != record.sha256 || found->relative_path != record.relative_path) {
        throw std::runtime_error("existing dataset ID conflicts with regenerated data: " +
                                 record.dataset_id);
    } else {
        *found = std::move(record);
    }
}

DatasetRecord materialize_record(std::filesystem::path const& root, DatasetRecord record) {
    const auto expected = regenerate(record);
    record.sha256 = sha256(expected);
    const auto path = resolved_path(root, record.relative_path);
    write_immutable_file(path, expected, record.sha256);
    return record;
}

} // namespace

Corpus::Corpus(std::filesystem::path root) : root_(std::move(root)) {}

std::vector<DatasetRecord> Corpus::generate_all() {
    auto records = load_existing_manifest(root_);
    for (auto record : required_datasets()) {
        merge_record(records, materialize_record(root_, std::move(record)));
    }
    write_manifest(manifest_path(root_), records);
    return records;
}

DatasetRecord Corpus::generate_one(std::string_view profile, std::size_t size) {
    const auto parsed = parse_profile(profile);
    if (!parsed || size > maximum_generated_size) {
        throw std::invalid_argument("unknown profile or size exceeds 256 MiB limit");
    }
    auto records = load_existing_manifest(root_);
    auto record = profile_dataset_spec(*parsed, size);
    record = materialize_record(root_, std::move(record));
    merge_record(records, record);
    write_manifest(manifest_path(root_), std::move(records));
    return record;
}

std::vector<DatasetRecord> Corpus::list() const {
    const auto path = manifest_path(root_);
    const auto bytes = read_binary_file(path);
    return parse_manifest(std::string(reinterpret_cast<char const*>(bytes.data()), bytes.size()));
}

std::vector<std::byte> Corpus::read_dataset(DatasetRecord const& record) const {
    if (record.size_bytes > maximum_generated_size) {
        throw std::runtime_error("dataset exceeds supported size limit");
    }
    const auto path = resolved_path(root_, record.relative_path);
    if (std::filesystem::file_size(path) != record.size_bytes) {
        throw std::runtime_error("dataset length does not match the manifest");
    }
    return read_binary_file(path);
}

ValidationResult Corpus::validate() const {
    ValidationResult result;
    bool files_readable = true;
    bool sizes_match = true;
    bool hashes_match = true;
    bool regeneration_match = true;
    bool IDs_paths_unique = true;
    bool schema_valid = true;
    bool unmodified = true;
    bool required_present = true;
    bool manifest_loaded = false;
    std::string detail;
    try {
        const auto corpus_manifest = manifest_path(root_);
        const auto manifest_bytes = read_binary_file(corpus_manifest);
        result.records = parse_manifest(std::string(
            reinterpret_cast<char const*>(manifest_bytes.data()), manifest_bytes.size()));
        manifest_loaded = true;
        const auto text = std::string(reinterpret_cast<char const*>(manifest_bytes.data()),
                                      manifest_bytes.size());
        const auto document = Json::parse(text);
        schema_valid = document.value("schema_version", 0) == 1 &&
                       document.value("generator_version", std::string{}) == generator_version;

        std::set<std::string> seen_ids;
        std::set<std::string> seen_paths;
        for (const auto& record : result.records) {
            if (!seen_ids.insert(record.dataset_id).second ||
                !seen_paths.insert(record.relative_path).second ||
                !is_safe_relative_path(record.relative_path)) {
                IDs_paths_unique = false;
            }
            try {
                if (record.size_bytes > maximum_generated_size) {
                    throw std::runtime_error("manifest dataset exceeds supported size limit");
                }
                const auto path = resolved_path(root_, record.relative_path);
                const auto file_size = std::filesystem::file_size(path);
                if (file_size != record.size_bytes) {
                    files_readable = false;
                    sizes_match = false;
                    hashes_match = false;
                    regeneration_match = false;
                    unmodified = false;
                    detail = "size mismatch: " + record.dataset_id;
                    continue;
                }
                const auto bytes = read_binary_file(path);
                if (bytes.size() != record.size_bytes) {
                    sizes_match = false;
                    detail = "size mismatch: " + record.dataset_id;
                }
                const auto hash = sha256(bytes);
                if (hash != record.sha256) {
                    hashes_match = false;
                    unmodified = false;
                    detail = "SHA-256 mismatch: " + record.dataset_id;
                }
                const auto regenerated = regenerate(record);
                if (regenerated != bytes) {
                    regeneration_match = false;
                    unmodified = false;
                    detail = "regenerated bytes differ: " + record.dataset_id;
                }
            } catch (std::exception const& error) {
                files_readable = false;
                sizes_match = false;
                hashes_match = false;
                regeneration_match = false;
                unmodified = false;
                detail = error.what();
            }
        }
        const auto required = required_datasets();
        for (const auto& item : required) {
            const auto found =
                std::find_if(result.records.begin(), result.records.end(), [&](auto const& record) {
                    return record.dataset_id == item.dataset_id;
                });
            if (found == result.records.end()) {
                required_present = false;
                break;
            }
        }
    } catch (std::exception const& error) {
        schema_valid = false;
        required_present = false;
        detail = error.what();
        if (detail.find("duplicate") != std::string::npos ||
            detail.find("unsafe relative path") != std::string::npos ||
            detail.find("empty ID") != std::string::npos) {
            IDs_paths_unique = false;
        }
    }

    const auto add = [&](std::string gate, std::string status, std::string explanation) {
        result.gates.push_back({std::move(gate), std::move(status), std::move(explanation)});
    };
    const auto gate_status = [](bool passed) { return passed ? "PASS" : "FAIL"; };
    if (manifest_loaded) {
        add("G01", gate_status(files_readable),
            files_readable ? "all manifest files were read" : detail);
        add("G02", gate_status(sizes_match),
            sizes_match ? "all file sizes match the manifest" : detail);
        add("G03", gate_status(hashes_match),
            hashes_match ? "all SHA-256 checksums match" : detail);
        add("G04", result.records.empty() ? "SKIP" : gate_status(regeneration_match),
            result.records.empty() ? "there are no manifest entries to regenerate"
            : regeneration_match   ? "regeneration reproduces exact bytes"
                                   : detail);
        add("G05", gate_status(IDs_paths_unique),
            IDs_paths_unique ? "IDs and paths are unique and safe" : detail);
        add("G06", gate_status(schema_valid),
            schema_valid ? "manifest schema version 1 is valid" : detail);
        add("G07", gate_status(unmodified),
            unmodified ? "generated files match their deterministic source" : detail);
    } else {
        add("G01", "SKIP", "manifest could not be parsed, so file reads were skipped");
        add("G02", "SKIP", "manifest could not be parsed, so size checks were skipped");
        add("G03", "SKIP", "manifest could not be parsed, so checksum checks were skipped");
        add("G04", "SKIP", "manifest could not be parsed, so regeneration was skipped");
        add("G05", IDs_paths_unique ? "SKIP" : "FAIL",
            IDs_paths_unique ? "manifest structure was unavailable" : detail);
        add("G06", "FAIL", detail.empty() ? "manifest is missing" : detail);
        add("G07", "SKIP", "manifest could not be parsed, so modification checks were skipped");
    }
    add("G08", gate_status(required_present),
        required_present ? "all 32 required profile-size cases and edge cases exist"
                         : "one or more required dataset IDs are missing or manifest is invalid");
    result.passed = std::all_of(result.gates.begin(), result.gates.end(),
                                [](auto const& gate) { return gate.status == "PASS"; });
    return result;
}

std::filesystem::path const& Corpus::root() const noexcept {
    return root_;
}

bool is_safe_relative_path(std::string_view path) noexcept {
    if (path.empty() || path.front() == '/' || path.front() == '\\' ||
        path.find('\\') != std::string_view::npos || path.find('\0') != std::string_view::npos ||
        path.find(':') != std::string_view::npos) {
        return false;
    }
    std::size_t component_start = 0;
    while (component_start < path.size()) {
        const auto separator = path.find('/', component_start);
        const auto component_end = separator == std::string_view::npos ? path.size() : separator;
        const auto component = path.substr(component_start, component_end - component_start);
        if (component.empty() || component == ".." || component == ".") {
            return false;
        }
        if (separator == std::string_view::npos) {
            return true;
        }
        component_start = separator + 1U;
    }
    return false;
}

std::vector<std::byte> read_binary_file(std::filesystem::path const& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path.string());
    }
    const auto end = input.tellg();
    if (end < 0 ||
        static_cast<std::uintmax_t>(end) >
            static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
        end > std::numeric_limits<std::streamsize>::max()) {
        throw std::runtime_error("file is too large to read safely: " + path.string());
    }
    const auto size = static_cast<std::size_t>(end);
    std::vector<std::byte> data(size);
    input.seekg(0, std::ios::beg);
    if (!data.empty()) {
        input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    if (!input || input.gcount() != static_cast<std::streamsize>(data.size())) {
        throw std::runtime_error("short read: " + path.string());
    }
    return data;
}

} // namespace shinshu
