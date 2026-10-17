#include "engine/core/Config.hpp"
#include "engine/core/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace classicfps::core {

namespace {

[[nodiscard]] std::string trim(std::string_view input) {
    std::size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
        ++start;
    }
    std::size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }
    return std::string(input.substr(start, end - start));
}

[[nodiscard]] std::string stripQuotes(std::string_view input) {
    if (input.size() >= 2 &&
        ((input.front() == '"' && input.back() == '"') ||
         (input.front() == '\'' && input.back() == '\''))) {
        return std::string(input.substr(1, input.size() - 2));
    }
    return std::string(input);
}

} // namespace

Config::Config() {
    loadDefaults();
}

void Config::loadDefaults() {
    values_.clear();
    // Window and display parameters (GoldSrc-inspired naming)
    setString("window_title", "ClassicFPS Engine v1.0 - Tactical FPS (All 8 Phases)");
    setInt("vid_width", 800);
    setInt("vid_height", 600);
    setBool("vid_fullscreen", false);
    setBool("vid_resizable", true);
    setBool("gl_vsync", true);
    setBool("r_allow_software_fallback", true);

    // Classic dark slate-blue/gray clear & fog color
    setFloat("r_clear_r", 0.08f);
    setFloat("r_clear_g", 0.11f);
    setFloat("r_clear_b", 0.15f);
    setFloat("r_clear_a", 1.0f);

    // Phase 2 & 3: Camera, Input, Fog & FPS Physics parameters
    setFloat("fov_degrees", 75.0f);
    setFloat("sensitivity", 0.15f);
    setBool("m_invert_pitch", false);
    setFloat("cl_yawspeed", 100.0f);
    setBool("cam_flythrough", true);
    setBool("r_fog_enable", true);
    setFloat("r_fog_start", 10.0f);
    setFloat("r_fog_end", 30.0f);
    setString("weapons_config", "game/configs/weapons.cfg");

    // FPS Player Physics (GoldSrc-inspired CVars)
    setFloat("sv_gravity", 20.0f);
    setFloat("pm_jump_speed", 6.8f);
    setFloat("pm_run_speed", 6.5f);
    setFloat("pm_walk_speed", 3.4f);
    setFloat("pm_crouch_speed", 2.4f);
    setFloat("sv_accelerate", 10.0f);
    setFloat("sv_airaccelerate", 2.5f);
    setFloat("sv_friction", 6.5f);
    setFloat("sv_stopspeed", 1.8f);
    setFloat("pm_step_height", 0.45f);
    setFloat("pm_eye_height_stand", 1.62f);
    setFloat("pm_eye_height_crouch", 0.85f);

    // Simulation and frame timing
    setFloat("sys_ticrate", 100.0f);
    setInt("fps_max", 0); // 0 = uncapped / VSync-driven

    // Diagnostics & logging
    setString("log_level", "info");
    setString("log_file", "");
}

bool Config::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        Logger::instance().warn("Config", "Could not open configuration file: " + filePath);
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const bool ok = loadFromString(buffer.str());
    if (ok) {
        Logger::instance().info("Config", "Loaded configuration from " + filePath);
    }
    return ok;
}

bool Config::loadFromString(std::string_view content) {
    std::istringstream stream{std::string(content)};
    std::string rawLine;
    std::size_t lineNumber = 0;

    while (std::getline(stream, rawLine)) {
        ++lineNumber;

        // Strip '#' or '//' comments outside quotes
        bool inQuotes = false;
        std::size_t commentPos = std::string::npos;
        for (std::size_t i = 0; i < rawLine.size(); ++i) {
            if (rawLine[i] == '"') {
                inQuotes = !inQuotes;
            } else if (!inQuotes) {
                if (rawLine[i] == '#') {
                    commentPos = i;
                    break;
                }
                if (rawLine[i] == '/' && (i + 1) < rawLine.size() && rawLine[i + 1] == '/') {
                    commentPos = i;
                    break;
                }
            }
        }

        const std::string_view uncommented = (commentPos == std::string::npos)
            ? std::string_view(rawLine)
            : std::string_view(rawLine).substr(0, commentPos);

        const std::string line = trim(uncommented);
        if (line.empty()) {
            continue;
        }

        std::string key;
        std::string value;

        const std::size_t eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            key = trim(std::string_view(line).substr(0, eqPos));
            value = stripQuotes(trim(std::string_view(line).substr(eqPos + 1)));
        } else {
            // Space-separated CVar syntax: key "value" or key value
            std::size_t splitPos = 0;
            while (splitPos < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[splitPos])) == 0) {
                ++splitPos;
            }
            if (splitPos == 0 || splitPos >= line.size()) {
                Logger::instance().warn(
                    "Config",
                    "Ignoring malformed config line " + std::to_string(lineNumber) + ": " + line);
                continue;
            }
            key = trim(std::string_view(line).substr(0, splitPos));
            value = stripQuotes(trim(std::string_view(line).substr(splitPos + 1)));
        }

        if (!key.empty()) {
            values_[key] = value;
        }
    }

    return true;
}

bool Config::saveToFile(const std::string& filePath) const {
    std::ofstream file(filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        Logger::instance().error("Config", "Failed to write configuration file: " + filePath);
        return false;
    }

    file << "# ClassicFPS Engine Configuration\n";
    std::vector<std::string> sortedKeys = keys();
    std::sort(sortedKeys.begin(), sortedKeys.end());

    for (const std::string& key : sortedKeys) {
        const auto it = values_.find(key);
        if (it != values_.end()) {
            file << key << " \"" << it->second << "\"\n";
        }
    }
    return file.good();
}

void Config::setString(std::string_view key, std::string_view value) {
    values_[std::string(key)] = std::string(value);
}

void Config::setInt(std::string_view key, int value) {
    values_[std::string(key)] = std::to_string(value);
}

void Config::setFloat(std::string_view key, float value) {
    std::ostringstream oss;
    oss << value;
    values_[std::string(key)] = oss.str();
}

void Config::setBool(std::string_view key, bool value) {
    setString(key, value ? std::string_view("1") : std::string_view("0"));
}

bool Config::contains(std::string_view key) const {
    return values_.find(std::string(key)) != values_.end();
}

std::string Config::getString(std::string_view key, std::string_view fallback) const {
    const auto it = values_.find(std::string(key));
    if (it == values_.end()) {
        return std::string(fallback);
    }
    return it->second;
}

int Config::getInt(std::string_view key, int fallback) const {
    const auto it = values_.find(std::string(key));
    if (it == values_.end() || it->second.empty()) {
        return fallback;
    }
    char* endPtr = nullptr;
    const long parsed = std::strtol(it->second.c_str(), &endPtr, 10);
    if (endPtr == it->second.c_str() || *endPtr != '\0') {
        return fallback;
    }
    return static_cast<int>(parsed);
}

float Config::getFloat(std::string_view key, float fallback) const {
    const auto it = values_.find(std::string(key));
    if (it == values_.end() || it->second.empty()) {
        return fallback;
    }
    char* endPtr = nullptr;
    const float parsed = std::strtof(it->second.c_str(), &endPtr);
    if (endPtr == it->second.c_str() || *endPtr != '\0') {
        return fallback;
    }
    return parsed;
}

bool Config::getBool(std::string_view key, bool fallback) const {
    const auto it = values_.find(std::string(key));
    if (it == values_.end() || it->second.empty()) {
        return fallback;
    }
    const std::string& v = it->second;
    if (v == "1" || v == "true" || v == "TRUE" || v == "yes" || v == "on") {
        return true;
    }
    if (v == "0" || v == "false" || v == "FALSE" || v == "no" || v == "off") {
        return false;
    }
    return fallback;
}

std::vector<std::string> Config::keys() const {
    std::vector<std::string> result;
    result.reserve(values_.size());
    for (const auto& [k, _] : values_) {
        result.push_back(k);
    }
    return result;
}

} // namespace classicfps::core
