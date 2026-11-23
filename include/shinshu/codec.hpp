#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace shinshu {

enum class CodecStatus { success, not_implemented, malformed_input, failure };

struct CodecResult {
    CodecStatus status{CodecStatus::failure};
    std::vector<std::byte> bytes;
    std::string_view error;
};

class CodecAdapter {
  public:
    virtual ~CodecAdapter() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual std::string_view version() const noexcept = 0;
    [[nodiscard]] virtual bool infrastructure_only() const noexcept = 0;
    [[nodiscard]] virtual CodecResult compress(std::span<std::byte const> input) const = 0;
    // Decoders must return malformed_input for corrupt or truncated streams.
    [[nodiscard]] virtual CodecResult decompress(std::span<std::byte const> input) const = 0;
};

[[nodiscard]] CodecAdapter const& identity_codec() noexcept;
[[nodiscard]] CodecAdapter const& unimplemented_codec(std::string_view name) noexcept;
[[nodiscard]] CodecAdapter const* find_codec_adapter(std::string_view name) noexcept;

} // namespace shinshu
