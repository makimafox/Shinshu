#include "shinshu/codec.hpp"

#include <array>
#include <string_view>
#include <vector>

namespace shinshu {
namespace {

class IdentityCodec final : public CodecAdapter {
  public:
    std::string_view name() const noexcept override {
        return "identity";
    }
    std::string_view version() const noexcept override {
        return "1";
    }
    bool infrastructure_only() const noexcept override {
        return true;
    }
    CodecResult compress(std::span<std::byte const> input) const override {
        return {CodecStatus::success, std::vector<std::byte>(input.begin(), input.end()), {}};
    }
    CodecResult decompress(std::span<std::byte const> input) const override {
        return {CodecStatus::success, std::vector<std::byte>(input.begin(), input.end()), {}};
    }
};

class UnimplementedCodec final : public CodecAdapter {
  public:
    explicit UnimplementedCodec(std::string_view codec_name) : name_(codec_name) {}
    std::string_view name() const noexcept override {
        return name_;
    }
    std::string_view version() const noexcept override {
        return "unavailable";
    }
    bool infrastructure_only() const noexcept override {
        return false;
    }
    CodecResult compress(std::span<std::byte const>) const override {
        return {CodecStatus::not_implemented, {}, "codec backend is not implemented"};
    }
    CodecResult decompress(std::span<std::byte const>) const override {
        return {CodecStatus::not_implemented, {}, "codec backend is not implemented"};
    }

  private:
    std::string_view name_;
};

const IdentityCodec identity_instance;
const std::array<std::string_view, 6> names{
    "gzip", "xz", "zstd", "neural", "neural-residual", "neural-probability"};
const std::array<UnimplementedCodec, 6> unavailable{UnimplementedCodec{"gzip"},
                                                    UnimplementedCodec{"xz"},
                                                    UnimplementedCodec{"zstd"},
                                                    UnimplementedCodec{"neural"},
                                                    UnimplementedCodec{"neural-residual"},
                                                    UnimplementedCodec{"neural-probability"}};

} // namespace

CodecAdapter const& identity_codec() noexcept {
    return identity_instance;
}

CodecAdapter const& unimplemented_codec(std::string_view name) noexcept {
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (names[index] == name)
            return unavailable[index];
    }
    return unavailable[0];
}

CodecAdapter const* find_codec_adapter(std::string_view name) noexcept {
    if (name == identity_instance.name())
        return &identity_instance;
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (names[index] == name)
            return &unavailable[index];
    }
    return nullptr;
}

} // namespace shinshu
