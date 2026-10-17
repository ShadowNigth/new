#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

namespace classicfps::core {

enum class ErrorCategory : std::uint8_t {
    None = 0,
    Config,
    Platform,
    Graphics,
    Resource,
    Runtime,
    Signal
};

struct EngineError {
    ErrorCategory category{ErrorCategory::None};
    int code{0};
    std::string subsystem;
    std::string message;

    [[nodiscard]] constexpr bool isOk() const noexcept {
        return category == ErrorCategory::None && code == 0;
    }

    [[nodiscard]] static EngineError ok() {
        return EngineError{};
    }
};

class ErrorHandler {
public:
    static void installSignalHandlers() noexcept;
    [[nodiscard]] static bool terminationRequested() noexcept;
    static void clearTerminationRequest() noexcept;

    static void reportCritical(
        ErrorCategory category,
        int code,
        std::string_view subsystem,
        std::string_view message);

    [[nodiscard]] static EngineError lastCriticalError();
    static void clearLastError();
    [[nodiscard]] static std::string_view categoryToString(ErrorCategory category) noexcept;

private:
    static void handleSignal(int sig) noexcept;
    static std::atomic<bool> terminationRequested_;
    static std::atomic<int> lastSignal_;
};

} // namespace classicfps::core
