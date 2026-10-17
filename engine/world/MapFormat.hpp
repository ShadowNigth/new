#pragma once

#include "engine/renderer/SceneTypes.hpp"
#include "engine/world/EntityManager.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::world {

struct MapBrushDef {
    std::string tag;
    bool isRampNegZ{false};
    glm::vec3 center{0.0f};
    glm::vec3 size{1.0f};
    std::string textureId{"concrete"};
    glm::vec3 tintColor{1.0f};
    float uvScalePerMeter{0.5f};
    bool hasCollision{true};
};

struct MapEntityDef {
    EntityType type{EntityType::TargetDummy};
    std::string name;
    glm::vec3 position{0.0f};
    float yawDegrees{0.0f};
    float maxHealth{100.0f};
    std::string targetTag;
    std::string customPayload;
};

struct MapData {
    std::string name{"de_arena"};
    std::string author{"ClassicFPS Engine"};
    std::uint32_t version{1};

    glm::vec3 ambientColor{0.34f, 0.37f, 0.43f};
    renderer::DirectionalLight sunLight{};
    renderer::FogSettings fog{};

    std::vector<MapBrushDef> brushes;
    std::vector<renderer::PointLight> pointLights;
    std::vector<MapEntityDef> entities;
};

class MapFormat {
public:
    static constexpr std::uint32_t kBinaryMagic = 0x4D424643U; // "CFBM"
    static constexpr std::uint32_t kBinaryVersion = 1U;

    // Text (.cfmap) serialization and parsing
    [[nodiscard]] static std::string serializeToText(const MapData& map);
    [[nodiscard]] static bool saveTextMap(const MapData& map, const std::string& filePath);
    [[nodiscard]] static bool loadTextMapFromString(std::string_view text, MapData& outMap);
    [[nodiscard]] static bool loadTextMap(const std::string& filePath, MapData& outMap);

    // Versioned Binary (.cbsp) serialization and parsing
    [[nodiscard]] static std::vector<std::uint8_t> serializeToBinary(const MapData& map);
    [[nodiscard]] static bool saveBinaryMap(const MapData& map, const std::string& filePath);
    [[nodiscard]] static bool loadBinaryMapFromMemory(
        const std::vector<std::uint8_t>& bytes,
        MapData& outMap);
    [[nodiscard]] static bool loadBinaryMap(const std::string& filePath, MapData& outMap);

    // Creates the default Tactical Arena MapData
    [[nodiscard]] static MapData createDefaultArenaMapData();
};

} // namespace classicfps::world
