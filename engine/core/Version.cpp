#include "engine/core/Version.hpp"

#include <sstream>

namespace classicfps::core {

std::string Version::toString() {
    constexpr VersionInfo info = current();
    std::ostringstream oss;
    oss << info.major << '.' << info.minor << '.' << info.patch << '-' << info.phase;
    return oss.str();
}

std::string Version::buildBanner() {
    constexpr VersionInfo info = current();
    std::ostringstream oss;
    oss << info.engineName << " v" << toString() << " [" << info.platform << ", C++"
        << __cplusplus << "]";
    return oss.str();
}

} // namespace classicfps::core
