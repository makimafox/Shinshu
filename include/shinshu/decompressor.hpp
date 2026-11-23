#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace shinshu {

// Contract for future decoders. Implementations must validate all encoded input.
class Decompressor {
  public:
    virtual ~Decompressor() = default;
    [[nodiscard]] virtual std::vector<std::byte>
    decompress(std::span<std::byte const> input) const = 0;
};

} // namespace shinshu
