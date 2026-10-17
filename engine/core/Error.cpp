#include "engine/core/Error.hpp"
#include "engine/core/Logger.hpp"

#include <csignal>
#include <mutex>
#include <sstream>

namespace classicfps::core {

namespace {
std::mutex g_errorMutex;
EngineError g_lastError{};
} // namespace

std::atomic<bool> ErrorHandler::terminationRequested_{false};
std::atomic<int> ErrorHandler::lastSignal_{0};

void ErrorHandler::handleSignal(int sig) noexcept {
    lastSignal_.store(sig, std::memory_order_relaxed);
    terminationRequested_.store(true, std::memory_order_relaxed);
}

void ErrorHandler::installSignalHandlers() noexcept {
    terminationRequested_.store(false, std::memory_order_relaxed);
    lastSignal_.store(0, std::memory_order_relaxed);
    std::signal(SIGINT, &ErrorHandler::handleSignal);
    std::signal(SIGTERM, &ErrorHandler::handleSignal);
}

bool ErrorHandler::terminationRequested() noexcept {
    return terminationRequested_.load(std::memory_order_relaxed);
}

void ErrorHandler::clearTerminationRequest() noexcept {
    terminationRequested_.store(false, std::memory_order_relaxed);
    lastSignal_.store(0, std::memory_order_relaxed);
}

void ErrorHandler::reportCritical(
    ErrorCategory category,
    int code,
    std::string_view subsystem,
    std::string_view message) {
    {
        std::scoped_lock lock(g_errorMutex);
        g_lastError = EngineError{
            .category = category,
            .code = code,
            .subsystem = std::string(subsystem),
            .message = std::string(message)
        };
    }

    std::ostringstream oss;
    oss << "Critical error [" << categoryToString(category) << " #" << code << "]: " << message;
    Logger::instance().fatal(subsystem, oss.str());
}

EngineError ErrorHandler::lastCriticalError() {
    std::scoped_lock lock(g_errorMutex);
    return g_lastError;
}

void ErrorHandler::clearLastError() {
    std::scoped_lock lock(g_errorMutex);
    g_lastError = EngineError{};
}

std::string_view ErrorHandler::categoryToString(ErrorCategory category) noexcept {
    switch (category) {
        case ErrorCategory::None:     return "None";
        case ErrorCategory::Config:   return "Config";
        case ErrorCategory::Platform: return "Platform";
        case ErrorCategory::Graphics: return "Graphics";
        case ErrorCategory::Resource: return "Resource";
        case ErrorCategory::Runtime:  return "Runtime";
        case ErrorCategory::Signal:   return "Signal";
    }
    return "Unknown";
}

} // namespace classicfps::core
