#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace shinshu {

// Contract for future encoders. No concrete compression algorithm is provided yet.
class Compressor {
  public:
    virtual ~Compressor() = default;
    [[nodiscard]] virtual std::vector<std::byte>
    compress(std::span<std::byte const> input) const = 0;
};

} // namespace shinshu
