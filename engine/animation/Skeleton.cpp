#include "engine/animation/Skeleton.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace classicfps::animation {

glm::mat4 TransformTRS::toMatrix() const noexcept {
    glm::mat4 mat = glm::translate(glm::mat4(1.0f), translation);
    mat *= glm::mat4_cast(glm::normalize(rotation));
    mat = glm::scale(mat, scale);
    return mat;
}

TransformTRS TransformTRS::blend(
    const TransformTRS& a,
    const TransformTRS& b,
    float factor) noexcept {
    const float t = std::clamp(factor, 0.0f, 1.0f);
    TransformTRS out{};
    out.translation = glm::mix(a.translation, b.translation, t);
    out.rotation = glm::normalize(glm::slerp(a.rotation, b.rotation, t));
    out.scale = glm::mix(a.scale, b.scale, t);
    return out;
}

TransformTRS BoneTrack::sample(float timeSeconds) const noexcept {
    if (keyframes.empty()) {
        return TransformTRS{};
    }
    if (keyframes.size() == 1 || timeSeconds <= keyframes.front().timeSeconds) {
        return keyframes.front().transform;
    }
    if (timeSeconds >= keyframes.back().timeSeconds) {
        return keyframes.back().transform;
    }

    for (std::size_t i = 0; i + 1 < keyframes.size(); ++i) {
        const Keyframe& k0 = keyframes[i];
        const Keyframe& k1 = keyframes[i + 1];
        if (timeSeconds >= k0.timeSeconds && timeSeconds <= k1.timeSeconds) {
            const float span = std::max(1e-5f, k1.timeSeconds - k0.timeSeconds);
            const float alpha = (timeSeconds - k0.timeSeconds) / span;
            return TransformTRS::blend(k0.transform, k1.transform, alpha);
        }
    }
    return keyframes.back().transform;
}

std::size_t Skeleton::addBone(
    std::string name,
    int parentIndex,
    const TransformTRS& bindLocal,
    const glm::mat4& inverseBind) {
    const std::size_t idx = bones_.size();
    bones_.push_back(Bone{
        .name = std::move(name),
        .parentIndex = parentIndex,
        .bindLocalTransform = bindLocal,
        .inverseBindMatrix = inverseBind
    });
    return idx;
}

void Skeleton::computeDefaultInverseBindMatrices() noexcept {
    std::vector<glm::mat4> globalBind(bones_.size(), glm::mat4(1.0f));
    for (std::size_t i = 0; i < bones_.size(); ++i) {
        const glm::mat4 localMat = bones_[i].bindLocalTransform.toMatrix();
        const int parent = bones_[i].parentIndex;
        if (parent >= 0 && static_cast<std::size_t>(parent) < i) {
            globalBind[i] = globalBind[static_cast<std::size_t>(parent)] * localMat;
        } else {
            globalBind[i] = localMat;
        }
        bones_[i].inverseBindMatrix = glm::inverse(globalBind[i]);
    }
}

void Skeleton::addClip(AnimationClip clip) {
    for (auto& existing : clips_) {
        if (existing.name == clip.name) {
            existing = std::move(clip);
            return;
        }
    }
    clips_.push_back(std::move(clip));
}

int Skeleton::findBoneIndex(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < bones_.size(); ++i) {
        if (bones_[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const AnimationClip* Skeleton::findClip(std::string_view name) const noexcept {
    for (const auto& clip : clips_) {
        if (clip.name == name) {
            return &clip;
        }
    }
    return nullptr;
}

void Skeleton::sampleLocalPose(
    const AnimationClip* clip,
    float timeSeconds,
    std::vector<TransformTRS>& outLocalPose) const noexcept {
    outLocalPose.resize(bones_.size());
    for (std::size_t i = 0; i < bones_.size(); ++i) {
        outLocalPose[i] = bones_[i].bindLocalTransform;
    }

    if (clip == nullptr) {
        return;
    }

    for (const BoneTrack& track : clip->tracks) {
        if (track.boneIndex < outLocalPose.size() && !track.keyframes.empty()) {
            outLocalPose[track.boneIndex] = track.sample(timeSeconds);
        }
    }
}

void Skeleton::computeGlobalAndSkinMatrices(
    const std::vector<TransformTRS>& localPose,
    std::vector<glm::mat4>& outGlobalMatrices,
    std::vector<glm::mat4>& outSkinMatrices) const noexcept {
    const std::size_t count = bones_.size();
    outGlobalMatrices.resize(count, glm::mat4(1.0f));
    outSkinMatrices.resize(count, glm::mat4(1.0f));

    for (std::size_t i = 0; i < count; ++i) {
        const glm::mat4 localMat =
            (i < localPose.size()) ? localPose[i].toMatrix() : bones_[i].bindLocalTransform.toMatrix();
        const int parent = bones_[i].parentIndex;
        if (parent >= 0 && static_cast<std::size_t>(parent) < count) {
            outGlobalMatrices[i] = outGlobalMatrices[static_cast<std::size_t>(parent)] * localMat;
        } else {
            outGlobalMatrices[i] = localMat;
        }
        outSkinMatrices[i] = outGlobalMatrices[i] * bones_[i].inverseBindMatrix;
    }
}

void Skeleton::applyLinearBlendSkinning(
    const std::vector<renderer::Vertex3D>& bindVertices,
    const std::vector<VertexSkinWeight>& skinWeights,
    const std::vector<glm::mat4>& skinMatrices,
    std::vector<renderer::Vertex3D>& outSkinnedVertices) noexcept {
    outSkinnedVertices.resize(bindVertices.size());
    if (skinMatrices.empty()) {
        outSkinnedVertices = bindVertices;
        return;
    }

    for (std::size_t v = 0; v < bindVertices.size(); ++v) {
        const renderer::Vertex3D& src = bindVertices[v];
        renderer::Vertex3D dst = src;

        if (v < skinWeights.size()) {
            const VertexSkinWeight& sw = skinWeights[v];
            glm::vec4 blendedPos(0.0f);
            glm::vec3 blendedNorm(0.0f);
            float totalWeight = 0.0f;

            for (std::size_t k = 0; k < 4; ++k) {
                const float w = sw.jointWeights[static_cast<glm::vec4::length_type>(k)];
                if (w <= 1e-5f) {
                    continue;
                }
                const std::uint16_t jIdx = sw.jointIndices[k];
                const glm::mat4& mat = (jIdx < skinMatrices.size())
                    ? skinMatrices[jIdx]
                    : glm::mat4(1.0f);

                blendedPos += (mat * glm::vec4(src.position, 1.0f)) * w;
                blendedNorm += (glm::mat3(mat) * src.normal) * w;
                totalWeight += w;
            }

            if (totalWeight > 1e-5f) {
                dst.position = glm::vec3(blendedPos);
                const float nLen = glm::length(blendedNorm);
                if (nLen > 1e-5f) {
                    dst.normal = blendedNorm / nLen;
                }
            }
        }

        outSkinnedVertices[v] = dst;
    }
}

AnimationPlayer::AnimationPlayer(const Skeleton* skeleton) noexcept {
    setSkeleton(skeleton);
}

void AnimationPlayer::setSkeleton(const Skeleton* skeleton) noexcept {
    skeleton_ = skeleton;
    activeClip_ = nullptr;
    prevClip_ = nullptr;
    currentTime_ = 0.0f;
    prevClipTime_ = 0.0f;
    blendTimer_ = 0.0f;
    blendDuration_ = 0.0f;
    finished_ = false;

    if (skeleton_ != nullptr) {
        skeleton_->sampleLocalPose(nullptr, 0.0f, localPose_);
        skeleton_->computeGlobalAndSkinMatrices(localPose_, globalMatrices_, skinMatrices_);
    } else {
        localPose_.clear();
        prevLocalPose_.clear();
        globalMatrices_.clear();
        skinMatrices_.clear();
    }
}

bool AnimationPlayer::play(
    std::string_view clipName,
    float blendDurationSeconds,
    bool restartIfSame) noexcept {
    if (skeleton_ == nullptr) {
        return false;
    }
    const AnimationClip* nextClip = skeleton_->findClip(clipName);
    if (nextClip == nullptr) {
        return false;
    }

    if (activeClip_ == nextClip && !restartIfSame && !finished_) {
        return true;
    }

    if (activeClip_ != nullptr && blendDurationSeconds > 1e-4f) {
        prevClip_ = activeClip_;
        prevClipTime_ = currentTime_;
        blendDuration_ = blendDurationSeconds;
        blendTimer_ = 0.0f;
    } else {
        prevClip_ = nullptr;
        blendDuration_ = 0.0f;
        blendTimer_ = 0.0f;
    }

    activeClip_ = nextClip;
    currentTime_ = 0.0f;
    finished_ = false;
    update(0.0f);
    return true;
}

void AnimationPlayer::update(float dt) noexcept {
    if (skeleton_ == nullptr) {
        return;
    }

    if (activeClip_ != nullptr) {
        currentTime_ += std::max(0.0f, dt);
        const float dur = std::max(1e-4f, activeClip_->durationSeconds);
        if (activeClip_->looping) {
            currentTime_ = std::fmod(currentTime_, dur);
        } else if (currentTime_ >= dur) {
            currentTime_ = dur;
            finished_ = true;
        }
    }

    skeleton_->sampleLocalPose(activeClip_, currentTime_, localPose_);

    if (prevClip_ != nullptr && blendDuration_ > 1e-4f) {
        blendTimer_ += std::max(0.0f, dt);
        const float alpha = std::clamp(blendTimer_ / blendDuration_, 0.0f, 1.0f);
        skeleton_->sampleLocalPose(prevClip_, prevClipTime_, prevLocalPose_);
        for (std::size_t i = 0; i < localPose_.size() && i < prevLocalPose_.size(); ++i) {
            localPose_[i] = TransformTRS::blend(prevLocalPose_[i], localPose_[i], alpha);
        }
        if (alpha >= 1.0f) {
            prevClip_ = nullptr;
        }
    }

    skeleton_->computeGlobalAndSkinMatrices(localPose_, globalMatrices_, skinMatrices_);
}

std::string_view AnimationPlayer::currentClipName() const noexcept {
    return (activeClip_ != nullptr) ? std::string_view(activeClip_->name) : std::string_view{};
}

} // namespace classicfps::animation
