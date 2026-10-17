#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace classicfps::core {

class Config {
public:
    Config();

    void loadDefaults();

    [[nodiscard]] bool loadFromFile(const std::string& filePath);
    [[nodiscard]] bool loadFromString(std::string_view content);
    [[nodiscard]] bool saveToFile(const std::string& filePath) const;

    void setString(std::string_view key, std::string_view value);
    void setInt(std::string_view key, int value);
    void setFloat(std::string_view key, float value);
    void setBool(std::string_view key, bool value);

    [[nodiscard]] bool contains(std::string_view key) const;
    [[nodiscard]] std::string getString(std::string_view key, std::string_view fallback = "") const;
    [[nodiscard]] int getInt(std::string_view key, int fallback = 0) const;
    [[nodiscard]] float getFloat(std::string_view key, float fallback = 0.0f) const;
    [[nodiscard]] bool getBool(std::string_view key, bool fallback = false) const;

    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
    [[nodiscard]] std::vector<std::string> keys() const;

private:
    std::unordered_map<std::string, std::string> values_;
};

} // namespace classicfps::core
