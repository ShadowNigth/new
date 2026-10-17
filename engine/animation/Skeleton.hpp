#pragma once

#include "engine/renderer/Mesh.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::animation {

// Up to 4 bone influences per vertex for Linear Blend Skinning (LBS)
struct VertexSkinWeight {
    std::array<std::uint16_t, 4> jointIndices{0, 0, 0, 0};
    glm::vec4 jointWeights{1.0f, 0.0f, 0.0f, 0.0f};
};

struct TransformTRS {
    glm::vec3 translation{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    [[nodiscard]] glm::mat4 toMatrix() const noexcept;
    [[nodiscard]] static TransformTRS blend(
        const TransformTRS& a,
        const TransformTRS& b,
        float factor) noexcept;
};

struct Bone {
    std::string name;
    int parentIndex{-1}; // -1 = root bone
    TransformTRS bindLocalTransform{};
    glm::mat4 inverseBindMatrix{1.0f};
};

struct Keyframe {
    float timeSeconds{0.0f};
    TransformTRS transform{};
};

struct BoneTrack {
    std::size_t boneIndex{0};
    std::vector<Keyframe> keyframes;

    [[nodiscard]] TransformTRS sample(float timeSeconds) const noexcept;
};

struct AnimationClip {
    std::string name;
    float durationSeconds{1.0f};
    bool looping{true};
    std::vector<BoneTrack> tracks;
};

class Skeleton {
public:
    Skeleton() = default;

    std::size_t addBone(
        std::string name,
        int parentIndex,
        const TransformTRS& bindLocal,
        const glm::mat4& inverseBind = glm::mat4(1.0f));

    void computeDefaultInverseBindMatrices() noexcept;
    void addClip(AnimationClip clip);

    [[nodiscard]] int findBoneIndex(std::string_view name) const noexcept;
    [[nodiscard]] const AnimationClip* findClip(std::string_view name) const noexcept;

    [[nodiscard]] std::size_t boneCount() const noexcept { return bones_.size(); }
    [[nodiscard]] const std::vector<Bone>& bones() const noexcept { return bones_; }
    [[nodiscard]] const std::vector<AnimationClip>& clips() const noexcept { return clips_; }

    void sampleLocalPose(
        const AnimationClip* clip,
        float timeSeconds,
        std::vector<TransformTRS>& outLocalPose) const noexcept;

    void computeGlobalAndSkinMatrices(
        const std::vector<TransformTRS>& localPose,
        std::vector<glm::mat4>& outGlobalMatrices,
        std::vector<glm::mat4>& outSkinMatrices) const noexcept;

    // Deforms bind-pose vertices and normals using 4-bone Linear Blend Skinning (LBS)
    static void applyLinearBlendSkinning(
        const std::vector<renderer::Vertex3D>& bindVertices,
        const std::vector<VertexSkinWeight>& skinWeights,
        const std::vector<glm::mat4>& skinMatrices,
        std::vector<renderer::Vertex3D>& outSkinnedVertices) noexcept;

private:
    std::vector<Bone> bones_;
    std::vector<AnimationClip> clips_;
};

class AnimationPlayer {
public:
    AnimationPlayer() = default;
    explicit AnimationPlayer(const Skeleton* skeleton) noexcept;

    void setSkeleton(const Skeleton* skeleton) noexcept;
    bool play(std::string_view clipName, float blendDurationSeconds = 0.12f, bool restartIfSame = false) noexcept;
    void update(float dt) noexcept;

    [[nodiscard]] std::string_view currentClipName() const noexcept;
    [[nodiscard]] float currentTime() const noexcept { return currentTime_; }
    [[nodiscard]] bool isPlaying() const noexcept { return activeClip_ != nullptr; }
    [[nodiscard]] bool isFinished() const noexcept { return finished_; }

    [[nodiscard]] const std::vector<glm::mat4>& globalBoneMatrices() const noexcept {
        return globalMatrices_;
    }
    [[nodiscard]] const std::vector<glm::mat4>& skinMatrices() const noexcept {
        return skinMatrices_;
    }

private:
    const Skeleton* skeleton_{nullptr};
    const AnimationClip* activeClip_{nullptr};
    const AnimationClip* prevClip_{nullptr};
    float currentTime_{0.0f};
    float prevClipTime_{0.0f};
    float blendTimer_{0.0f};
    float blendDuration_{0.0f};
    bool finished_{false};

    std::vector<TransformTRS> localPose_;
    std::vector<TransformTRS> prevLocalPose_;
    std::vector<glm::mat4> globalMatrices_;
    std::vector<glm::mat4> skinMatrices_;
};

} // namespace classicfps::animation
