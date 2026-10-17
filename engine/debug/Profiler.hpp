#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace classicfps::debug {

struct ProfileSectionStats {
    std::string name;
    std::uint64_t callCount{0};
    double totalMicroseconds{0.0};
    double maxMicroseconds{0.0};

    [[nodiscard]] double averageMicroseconds() const noexcept {
        return (callCount > 0) ? (totalMicroseconds / static_cast<double>(callCount)) : 0.0;
    }
};

class Profiler {
public:
    static Profiler& instance() noexcept;

    void reset() noexcept;
    void recordSample(std::string_view sectionName, double microseconds);

    [[nodiscard]] ProfileSectionStats sectionStats(std::string_view sectionName) const;
    [[nodiscard]] std::vector<ProfileSectionStats> allSections() const;
    [[nodiscard]] std::string summaryReport() const;

private:
    Profiler() = default;
    std::unordered_map<std::string, ProfileSectionStats> sections_;
};

class ScopedProfileTimer {
public:
    explicit ScopedProfileTimer(std::string_view sectionName) noexcept
        : sectionName_(sectionName),
          start_(std::chrono::steady_clock::now()) {}

    ~ScopedProfileTimer() {
        const auto end = std::chrono::steady_clock::now();
        const double us =
            std::chrono::duration<double, std::micro>(end - start_).count();
        Profiler::instance().recordSample(sectionName_, us);
    }

    ScopedProfileTimer(const ScopedProfileTimer&) = delete;
    ScopedProfileTimer& operator=(const ScopedProfileTimer&) = delete;

private:
    std::string_view sectionName_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace classicfps::debug
