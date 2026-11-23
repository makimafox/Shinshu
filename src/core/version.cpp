#include "shinshu/version.hpp"

#ifndef SHINSHU_VERSION
#define SHINSHU_VERSION "0.0.0"
#endif

namespace shinshu {

std::string_view version() noexcept {
    return SHINSHU_VERSION;
}

} // namespace shinshu
