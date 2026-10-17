#pragma once

#include "engine/core/ResourceManager.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::renderer {

class Texture2D final : public core::IResource {
public:
    Texture2D() = default;
    Texture2D(int width, int height, std::vector<std::uint8_t> rgbaPixels);
    ~Texture2D() override = default;

    [[nodiscard]] std::string_view typeName() const noexcept override { return "Texture2D"; }
    [[nodiscard]] std::size_t memoryBytes() const noexcept override { return pixels_.size(); }

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] const std::vector<std::uint8_t>& pixels() const noexcept { return pixels_; }
    [[nodiscard]] bool isValid() const noexcept {
        return width_ > 0 && height_ > 0 &&
               pixels_.size() == static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4U;
    }

    // Nearest-neighbor sampling with repeat wrap (classic pixelated look)
    [[nodiscard]] glm::vec4 sampleNearest(float u, float v) const noexcept;

    [[nodiscard]] bool loadFromPPM(const std::string& filePath);
    [[nodiscard]] bool saveToPPM(const std::string& filePath) const;

    // Procedural classic 64x64 low-res textures inspired by 90s WAD aesthetics
    [[nodiscard]] static std::shared_ptr<Texture2D> createCheckerboard(int size = 64, int tileSize = 8);
    [[nodiscard]] static std::shared_ptr<Texture2D> createConcreteWall(int size = 64);
    [[nodiscard]] static std::shared_ptr<Texture2D> createBrickWall(int size = 64);
    [[nodiscard]] static std::shared_ptr<Texture2D> createFloorTile(int size = 64);
    [[nodiscard]] static std::shared_ptr<Texture2D> createCeilingPanel(int size = 64);
    [[nodiscard]] static std::shared_ptr<Texture2D> createHazardStripes(int size = 64);
    [[nodiscard]] static std::shared_ptr<Texture2D> createCrateWood(int size = 64);

private:
    int width_{0};
    int height_{0};
    std::vector<std::uint8_t> pixels_;
};

} // namespace classicfps::renderer
