#pragma once

#include <cstdint>

namespace shinshu {

// Framework-independent inference contract for future neural model experiments.
class NeuralModel {
  public:
    virtual ~NeuralModel() = default;
    [[nodiscard]] virtual std::uint8_t predict_byte(std::uint64_t position) const = 0;
};

} // namespace shinshu
