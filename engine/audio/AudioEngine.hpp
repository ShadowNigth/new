#pragma once

#include "engine/core/ResourceManager.hpp"

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace classicfps::audio {

class SoundClip final : public core::IResource {
public:
    SoundClip() = default;
    SoundClip(std::uint32_t sampleRate, std::uint16_t channels, std::vector<std::int16_t> pcmSamples);

    [[nodiscard]] std::string_view typeName() const noexcept override { return "SoundClip"; }
    [[nodiscard]] std::size_t memoryBytes() const noexcept override {
        return samples_.size() * sizeof(std::int16_t);
    }

    [[nodiscard]] bool loadFromWavFile(const std::string& filePath);
    [[nodiscard]] bool saveToWavFile(const std::string& filePath) const;

    [[nodiscard]] static std::shared_ptr<SoundClip> createGunshotSound(std::uint32_t sampleRate = 22050);
    [[nodiscard]] static std::shared_ptr<SoundClip> createReloadSound(std::uint32_t sampleRate = 22050);
    [[nodiscard]] static std::shared_ptr<SoundClip> createDryFireSound(std::uint32_t sampleRate = 22050);
    [[nodiscard]] static std::shared_ptr<SoundClip> createFootstepSound(std::uint32_t sampleRate = 22050);
    [[nodiscard]] static std::shared_ptr<SoundClip> createImpactSound(std::uint32_t sampleRate = 22050);

    [[nodiscard]] std::uint32_t sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] const std::vector<std::int16_t>& samples() const noexcept { return samples_; }
    [[nodiscard]] float durationSeconds() const noexcept;

private:
    std::uint32_t sampleRate_{22050};
    std::uint16_t channels_{1};
    std::vector<std::int16_t> samples_;
};

struct ActiveSoundVoice {
    std::shared_ptr<SoundClip> clip;
    std::size_t sampleCursor{0};
    float baseVolume{1.0f};
    bool spatial3D{false};
    glm::vec3 worldPosition{0.0f};
    float minDistance{1.5f};
    float maxDistance{35.0f};
};

class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool initialize(core::ResourceManager& resources, bool headlessOrSoftwareMix = false);
    void shutdown() noexcept;

    void registerClip(const std::string& id, std::shared_ptr<SoundClip> clip);
    [[nodiscard]] std::shared_ptr<SoundClip> findClip(const std::string& id) const;

    void setMasterVolume(float volume) noexcept;
    [[nodiscard]] float masterVolume() const noexcept { return masterVolume_; }

    void setListenerTransform(
        const glm::vec3& position,
        const glm::vec3& forward,
        const glm::vec3& right,
        const glm::vec3& up) noexcept;

    bool playSound2D(const std::string& clipId, float volume = 1.0f);
    bool playSound3D(
        const std::string& clipId,
        const glm::vec3& position,
        float volume = 1.0f,
        float minDistance = 1.5f,
        float maxDistance = 35.0f);

    // Mixes active 2D and 3D voices into an interleaved stereo 16-bit PCM buffer
    void mixStereoFrames(std::int16_t* outInterleavedStereo, std::size_t frameCount) noexcept;

    [[nodiscard]] std::size_t activeVoiceCount() const noexcept;
    [[nodiscard]] std::uint64_t totalSoundsPlayed() const noexcept { return totalSoundsPlayed_; }
    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

private:
    static void sdlAudioCallback(void* userdata, std::uint8_t* stream, int len) noexcept;

    mutable std::mutex mutex_;
    bool initialized_{false};
    std::uint32_t sdlDeviceId_{0};
    std::uint32_t outputSampleRate_{22050};
    float masterVolume_{0.85f};

    glm::vec3 listenerPos_{0.0f};
    glm::vec3 listenerForward_{0.0f, 0.0f, -1.0f};
    glm::vec3 listenerRight_{1.0f, 0.0f, 0.0f};
    glm::vec3 listenerUp_{0.0f, 1.0f, 0.0f};

    std::unordered_map<std::string, std::shared_ptr<SoundClip>> clips_;
    std::vector<ActiveSoundVoice> activeVoices_;
    std::uint64_t totalSoundsPlayed_{0};
};

} // namespace classicfps::audio
