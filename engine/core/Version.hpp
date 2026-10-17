#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace classicfps::core {

struct VersionInfo {
    std::uint32_t major{1};
    std::uint32_t minor{0};
    std::uint32_t patch{0};
    std::string_view phase{"phase8-complete"};
    std::string_view engineName{"ClassicFPS Engine"};
    std::string_view platform{"Linux"};

    [[nodiscard]] constexpr std::uint32_t packed() const noexcept {
        return (major << 24U) | (minor << 16U) | patch;
    }

    [[nodiscard]] constexpr bool isCompatibleWith(const VersionInfo& other) const noexcept {
        return major == other.major && minor == other.minor;
    }
};

class Version {
public:
    [[nodiscard]] static constexpr VersionInfo current() noexcept {
        return VersionInfo{};
    }

    [[nodiscard]] static std::string toString();
    [[nodiscard]] static std::string buildBanner();
};

} // namespace classicfps::core
