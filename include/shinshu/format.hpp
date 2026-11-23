#pragma once

#include <cstdint>

namespace shinshu {

// In-memory description only. Wire encoding and validation are not implemented.
struct FileHeader {
    std::uint16_t format_version{1};
    std::uint64_t original_size{0};
    std::uint32_t flags{0};
};

} // namespace shinshu
