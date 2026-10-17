#include "engine/core/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace classicfps::core {

namespace {

[[nodiscard]] char toLowerAscii(char c) noexcept {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

} // namespace

Logger& Logger::instance() noexcept {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    closeLogFile();
}

void Logger::setMinLevel(LogLevel level) noexcept {
    std::scoped_lock lock(mutex_);
    minLevel_ = level;
}

LogLevel Logger::minLevel() const noexcept {
    std::scoped_lock lock(mutex_);
    return minLevel_;
}

void Logger::setConsoleOutputEnabled(bool enabled) noexcept {
    std::scoped_lock lock(mutex_);
    consoleOutputEnabled_ = enabled;
}

bool Logger::openLogFile(const std::string& filePath) {
    std::scoped_lock lock(mutex_);
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
    fileStream_.open(filePath, std::ios::out | std::ios::trunc);
    return fileStream_.is_open();
}

void Logger::closeLogFile() noexcept {
    std::scoped_lock lock(mutex_);
    if (fileStream_.is_open()) {
        fileStream_.flush();
        fileStream_.close();
    }
}

void Logger::log(LogLevel level, std::string_view subsystem, std::string_view message) {
    const std::string ts = formatTimestamp();

    std::scoped_lock lock(mutex_);
    if (level >= LogLevel::Error) {
        ++errorCount_;
    }

    if (static_cast<std::uint8_t>(level) < static_cast<std::uint8_t>(minLevel_)) {
        return;
    }

    LogEntry entry{
        .level = level,
        .timestamp = ts,
        .subsystem = std::string(subsystem),
        .message = std::string(message)
    };

    if (history_.size() >= maxHistoryEntries_) {
        history_.erase(history_.begin());
    }
    history_.push_back(entry);

    const std::string_view levelStr = levelToString(level);
    if (consoleOutputEnabled_) {
        std::ostream& out = (level >= LogLevel::Error) ? std::cerr : std::cout;
        out << '[' << entry.timestamp << "] [" << levelStr << "] [" << entry.subsystem << "] "
            << entry.message << '\n';
        if (level >= LogLevel::Warn) {
            out.flush();
        }
    }

    if (fileStream_.is_open()) {
        fileStream_ << '[' << entry.timestamp << "] [" << levelStr << "] [" << entry.subsystem
                    << "] " << entry.message << '\n';
        if (level >= LogLevel::Warn) {
            fileStream_.flush();
        }
    }
}

void Logger::trace(std::string_view subsystem, std::string_view message) {
    log(LogLevel::Trace, subsystem, message);
}

void Logger::debug(std::string_view subsystem, std::string_view message) {
    log(LogLevel::Debug, subsystem, message);
}

void Logger::info(std::string_view subsystem, std::string_view message) {
    log(LogLevel::Info, subsystem, message);
}

void Logger::warn(std::string_view subsystem, std::string_view message) {
    log(LogLevel::Warn, subsystem, message);
}

void Logger::error(std::string_view subsystem, std::string_view message) {
    log(LogLevel::Error, subsystem, message);
}

void Logger::fatal(std::string_view subsystem, std::string_view message) {
    log(LogLevel::Fatal, subsystem, message);
}

std::vector<LogEntry> Logger::recentEntries() const {
    std::scoped_lock lock(mutex_);
    return history_;
}

std::size_t Logger::errorCount() const noexcept {
    std::scoped_lock lock(mutex_);
    return errorCount_;
}

void Logger::clearHistory() noexcept {
    std::scoped_lock lock(mutex_);
    history_.clear();
    errorCount_ = 0;
}

std::string_view Logger::levelToString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "INFO ";
}

LogLevel Logger::parseLevel(std::string_view text, LogLevel fallback) noexcept {
    std::string lower(text.size(), '\0');
    std::transform(text.begin(), text.end(), lower.begin(), toLowerAscii);

    if (lower == "trace") return LogLevel::Trace;
    if (lower == "debug") return LogLevel::Debug;
    if (lower == "info")  return LogLevel::Info;
    if (lower == "warn" || lower == "warning") return LogLevel::Warn;
    if (lower == "error") return LogLevel::Error;
    if (lower == "fatal") return LogLevel::Fatal;
    return fallback;
}

std::string Logger::formatTimestamp() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t nowTime = system_clock::to_time_t(now);

    std::tm utcTm{};
    gmtime_r(&nowTime, &utcTm);

    std::ostringstream oss;
    oss << std::put_time(&utcTm, "%H:%M:%S") << '.'
        << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

} // namespace classicfps::core
