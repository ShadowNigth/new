#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::core {

enum class LogLevel : std::uint8_t {
    Trace = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4,
    Fatal = 5
};

struct LogEntry {
    LogLevel level{LogLevel::Info};
    std::string timestamp;
    std::string subsystem;
    std::string message;
};

class Logger {
public:
    static Logger& instance() noexcept;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void setMinLevel(LogLevel level) noexcept;
    [[nodiscard]] LogLevel minLevel() const noexcept;

    void setConsoleOutputEnabled(bool enabled) noexcept;
    [[nodiscard]] bool openLogFile(const std::string& filePath);
    void closeLogFile() noexcept;

    void log(LogLevel level, std::string_view subsystem, std::string_view message);

    void trace(std::string_view subsystem, std::string_view message);
    void debug(std::string_view subsystem, std::string_view message);
    void info(std::string_view subsystem, std::string_view message);
    void warn(std::string_view subsystem, std::string_view message);
    void error(std::string_view subsystem, std::string_view message);
    void fatal(std::string_view subsystem, std::string_view message);

    [[nodiscard]] std::vector<LogEntry> recentEntries() const;
    [[nodiscard]] std::size_t errorCount() const noexcept;
    void clearHistory() noexcept;

    [[nodiscard]] static std::string_view levelToString(LogLevel level) noexcept;
    [[nodiscard]] static LogLevel parseLevel(std::string_view text, LogLevel fallback = LogLevel::Info) noexcept;

private:
    Logger() = default;
    ~Logger();

    [[nodiscard]] static std::string formatTimestamp();

    mutable std::mutex mutex_;
    LogLevel minLevel_{LogLevel::Info};
    bool consoleOutputEnabled_{true};
    std::ofstream fileStream_;
    std::vector<LogEntry> history_;
    std::size_t maxHistoryEntries_{512};
    std::size_t errorCount_{0};
};

} // namespace classicfps::core
