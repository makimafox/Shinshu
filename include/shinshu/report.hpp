#pragma once

#include "shinshu/corpus.hpp"
#include "shinshu/roundtrip.hpp"

#include <filesystem>
#include <vector>

namespace shinshu {

void write_validation_report(std::filesystem::path const& root, ValidationResult const& result);
void write_roundtrip_records(std::filesystem::path const& root,
                             std::vector<RoundTripRecord> const& records);
[[nodiscard]] std::vector<RoundTripRecord>
load_roundtrip_records(std::filesystem::path const& root);
void write_roundtrip_reports(std::filesystem::path const& root,
                             std::vector<RoundTripRecord> const& records);

} // namespace shinshu
