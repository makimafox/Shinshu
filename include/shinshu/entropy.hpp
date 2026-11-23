#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace shinshu {

// Future deterministic entropy coding interface; probability representation is TBD.
class EntropyCoder {
  public:
    virtual ~EntropyCoder() = default;
    [[nodiscard]] virtual std::vector<std::byte> encode(std::span<std::byte const> input) const = 0;
    [[nodiscard]] virtual std::vector<std::byte> decode(std::span<std::byte const> input) const = 0;
};

} // namespace shinshu
