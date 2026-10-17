#include "engine/debug/Profiler.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace classicfps::debug {

Profiler& Profiler::instance() noexcept {
    static Profiler inst;
    return inst;
}

void Profiler::reset() noexcept {
    sections_.clear();
}

void Profiler::recordSample(std::string_view sectionName, double microseconds) {
    const std::string key(sectionName);
    auto& st = sections_[key];
    if (st.name.empty()) {
        st.name = key;
    }
    ++st.callCount;
    st.totalMicroseconds += microseconds;
    st.maxMicroseconds = std::max(st.maxMicroseconds, microseconds);
}

ProfileSectionStats Profiler::sectionStats(std::string_view sectionName) const {
    const auto it = sections_.find(std::string(sectionName));
    if (it != sections_.end()) {
        return it->second;
    }
    return ProfileSectionStats{.name = std::string(sectionName)};
}

std::vector<ProfileSectionStats> Profiler::allSections() const {
    std::vector<ProfileSectionStats> out;
    out.reserve(sections_.size());
    for (const auto& [_, st] : sections_) {
        out.push_back(st);
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return a.totalMicroseconds > b.totalMicroseconds;
    });
    return out;
}

std::string Profiler::summaryReport() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    const auto list = allSections();
    for (std::size_t i = 0; i < list.size(); ++i) {
        const auto& s = list[i];
        oss << s.name << "(avg=" << s.averageMicroseconds() << "us max="
            << s.maxMicroseconds << "us n=" << s.callCount << ")"
            << (i + 1 < list.size() ? ", " : "");
    }
    return oss.str();
}

} // namespace classicfps::debug
