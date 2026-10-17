#pragma once

#include "engine/animation/Skeleton.hpp"
#include "engine/core/ResourceManager.hpp"
#include "engine/renderer/Mesh.hpp"

#include <cstddef>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace classicfps::resources {

struct ModelSubMesh {
    std::string name;
    int attachedBoneIndex{0};
    glm::vec3 baseColorTint{1.0f, 1.0f, 1.0f};
    std::shared_ptr<renderer::Mesh> mesh;
};

class ArticulatedModel final : public core::IResource {
public:
    ArticulatedModel() = default;

    [[nodiscard]] std::string_view typeName() const noexcept override { return "ArticulatedModel"; }
    [[nodiscard]] std::size_t memoryBytes() const noexcept override;

    std::vector<ModelSubMesh> subMeshes;
    animation::Skeleton skeleton;
};

class ModelLoader {
public:
    // Loads a glTF 2.0 (.gltf with embedded base64 buffer) model from disk or string
    [[nodiscard]] static std::shared_ptr<ArticulatedModel> loadGltf2FromFile(
        const std::string& filePath);
    [[nodiscard]] static std::shared_ptr<ArticulatedModel> loadGltf2FromString(
        std::string_view gltfJson);

    // Saves an ArticulatedModel to a standard glTF 2.0 (.gltf with embedded base64 buffer) file
    [[nodiscard]] static bool saveGltf2ToFile(
        const ArticulatedModel& model,
        const std::string& filePath);
    [[nodiscard]] static std::string serializeGltf2ToString(const ArticulatedModel& model);

    // Loads a Wavefront .obj mesh into a single-bone ArticulatedModel
    [[nodiscard]] static std::shared_ptr<ArticulatedModel> loadObjFromString(
        std::string_view objContent,
        std::string_view name = "obj_mesh");

    // Procedural builders for original ClassicFPS low-poly models (used to generate/verify assets)
    [[nodiscard]] static std::shared_ptr<ArticulatedModel> createTacticalCarbineModel();
    [[nodiscard]] static std::shared_ptr<ArticulatedModel> createTargetDummyModel();
};

// Deduplicating asset loader & cache backed by ResourceManager
class AssetCache {
public:
    explicit AssetCache(core::ResourceManager& resourceManager) noexcept
        : resources_(resourceManager) {}

    [[nodiscard]] std::shared_ptr<ArticulatedModel> loadOrGetGltfModel(const std::string& filePath);

    [[nodiscard]] std::uint64_t cacheHits() const noexcept { return cacheHits_; }
    [[nodiscard]] std::uint64_t cacheMisses() const noexcept { return cacheMisses_; }

private:
    core::ResourceManager& resources_;
    std::uint64_t cacheHits_{0};
    std::uint64_t cacheMisses_{0};
};

} // namespace classicfps::resources
