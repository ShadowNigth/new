#include "engine/audio/AudioEngine.hpp"
#include "engine/core/Logger.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace classicfps::audio {

namespace {

#pragma pack(push, 1)
struct WavHeader {
    char riffId[4]{'R', 'I', 'F', 'F'};
    std::uint32_t riffSize{0};
    char waveId[4]{'W', 'A', 'V', 'E'};
    char fmtId[4]{'f', 'm', 't', ' '};
    std::uint32_t fmtSize{16};
    std::uint16_t audioFormat{1}; // PCM = 1
    std::uint16_t numChannels{1};
    std::uint32_t sampleRate{22050};
    std::uint32_t byteRate{44100};
    std::uint16_t blockAlign{2};
    std::uint16_t bitsPerSample{16};
    char dataId[4]{'d', 'a', 't', 'a'};
    std::uint32_t dataSize{0};
};
#pragma pack(pop)

[[nodiscard]] float hashWhiteNoise(std::uint32_t i) noexcept {
    std::uint32_t n = i * 747796405U + 2891336453U;
    n = ((n >> ((n >> 28U) + 4U)) ^ n) * 277803737U;
    n = (n >> 22U) ^ n;
    return (static_cast<float>(n & 0xFFFFU) / 32767.5f) - 1.0f;
}

} // namespace

SoundClip::SoundClip(
    std::uint32_t sampleRate,
    std::uint16_t channels,
    std::vector<std::int16_t> pcmSamples)
    : sampleRate_(sampleRate),
      channels_(channels),
      samples_(std::move(pcmSamples)) {}

bool SoundClip::saveToWavFile(const std::string& filePath) const {
    if (samples_.empty() || channels_ == 0 || sampleRate_ == 0) {
        return false;
    }

    WavHeader hdr{};
    hdr.numChannels = channels_;
    hdr.sampleRate = sampleRate_;
    hdr.bitsPerSample = 16;
    hdr.blockAlign = static_cast<std::uint16_t>(channels_ * sizeof(std::int16_t));
    hdr.byteRate = sampleRate_ * hdr.blockAlign;
    hdr.dataSize = static_cast<std::uint32_t>(samples_.size() * sizeof(std::int16_t));
    hdr.riffSize = 36U + hdr.dataSize;

    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(WavHeader));
    out.write(reinterpret_cast<const char*>(samples_.data()), hdr.dataSize);
    return out.good();
}

bool SoundClip::loadFromWavFile(const std::string& filePath) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    WavHeader hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(WavHeader));
    if (!in.good()) {
        return false;
    }

    if (std::memcmp(hdr.riffId, "RIFF", 4) != 0 ||
        std::memcmp(hdr.waveId, "WAVE", 4) != 0 ||
        hdr.audioFormat != 1 ||
        hdr.bitsPerSample != 16 ||
        hdr.numChannels == 0 ||
        hdr.dataSize == 0) {
        return false;
    }

    sampleRate_ = hdr.sampleRate;
    channels_ = hdr.numChannels;
    const std::size_t sampleCount = hdr.dataSize / sizeof(std::int16_t);
    samples_.resize(sampleCount);
    in.read(reinterpret_cast<char*>(samples_.data()), static_cast<std::streamsize>(sampleCount * sizeof(std::int16_t)));
    return in.good() || in.eof();
}

float SoundClip::durationSeconds() const noexcept {
    if (sampleRate_ == 0 || channels_ == 0) {
        return 0.0f;
    }
    return static_cast<float>(samples_.size()) /
           static_cast<float>(sampleRate_ * static_cast<std::uint32_t>(channels_));
}

std::shared_ptr<SoundClip> SoundClip::createGunshotSound(std::uint32_t sampleRate) {
    const std::size_t count = static_cast<std::size_t>(static_cast<float>(sampleRate) * 0.22f);
    std::vector<std::int16_t> pcm(count);
    for (std::size_t i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const float env = std::exp(-t * 24.0f);
        const float punch = std::sin(2.0f * 3.14159265f * (140.0f - t * 380.0f) * t);
        const float crack = hashWhiteNoise(static_cast<std::uint32_t>(i));
        const float val = std::clamp((punch * 0.55f + crack * 0.65f) * env, -1.0f, 1.0f);
        pcm[i] = static_cast<std::int16_t>(val * 28000.0f);
    }
    return std::make_shared<SoundClip>(sampleRate, 1, std::move(pcm));
}

std::shared_ptr<SoundClip> SoundClip::createReloadSound(std::uint32_t sampleRate) {
    const std::size_t count = static_cast<std::size_t>(static_cast<float>(sampleRate) * 0.35f);
    std::vector<std::int16_t> pcm(count, 0);
    for (std::size_t i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        // Two metallic clicks at t=0.04s and t=0.22s
        float env = 0.0f;
        if (t >= 0.04f && t < 0.12f) {
            env = std::exp(-(t - 0.04f) * 55.0f);
        } else if (t >= 0.22f && t < 0.32f) {
            env = std::exp(-(t - 0.22f) * 45.0f);
        }
        const float tone = std::sin(2.0f * 3.14159265f * 880.0f * t) * 0.5f +
                           hashWhiteNoise(static_cast<std::uint32_t>(i * 3U)) * 0.5f;
        const float val = std::clamp(tone * env, -1.0f, 1.0f);
        pcm[i] = static_cast<std::int16_t>(val * 22000.0f);
    }
    return std::make_shared<SoundClip>(sampleRate, 1, std::move(pcm));
}

std::shared_ptr<SoundClip> SoundClip::createDryFireSound(std::uint32_t sampleRate) {
    const std::size_t count = static_cast<std::size_t>(static_cast<float>(sampleRate) * 0.08f);
    std::vector<std::int16_t> pcm(count);
    for (std::size_t i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const float env = std::exp(-t * 80.0f);
        const float tone = std::sin(2.0f * 3.14159265f * 1450.0f * t);
        pcm[i] = static_cast<std::int16_t>(std::clamp(tone * env, -1.0f, 1.0f) * 20000.0f);
    }
    return std::make_shared<SoundClip>(sampleRate, 1, std::move(pcm));
}

std::shared_ptr<SoundClip> SoundClip::createFootstepSound(std::uint32_t sampleRate) {
    const std::size_t count = static_cast<std::size_t>(static_cast<float>(sampleRate) * 0.10f);
    std::vector<std::int16_t> pcm(count);
    for (std::size_t i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const float env = std::exp(-t * 42.0f);
        const float thud = std::sin(2.0f * 3.14159265f * 95.0f * t) * 0.6f +
                           hashWhiteNoise(static_cast<std::uint32_t>(i)) * 0.4f;
        pcm[i] = static_cast<std::int16_t>(std::clamp(thud * env, -1.0f, 1.0f) * 18000.0f);
    }
    return std::make_shared<SoundClip>(sampleRate, 1, std::move(pcm));
}

std::shared_ptr<SoundClip> SoundClip::createImpactSound(std::uint32_t sampleRate) {
    const std::size_t count = static_cast<std::size_t>(static_cast<float>(sampleRate) * 0.14f);
    std::vector<std::int16_t> pcm(count);
    for (std::size_t i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const float env = std::exp(-t * 36.0f);
        const float ping = std::sin(2.0f * 3.14159265f * (1200.0f + t * 1800.0f) * t) * 0.4f +
                           hashWhiteNoise(static_cast<std::uint32_t>(i * 7U)) * 0.6f;
        pcm[i] = static_cast<std::int16_t>(std::clamp(ping * env, -1.0f, 1.0f) * 21000.0f);
    }
    return std::make_shared<SoundClip>(sampleRate, 1, std::move(pcm));
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize(core::ResourceManager& resources, bool headlessOrSoftwareMix) {
    shutdown();

    // Register standard procedural FPS sound clips
    auto gunshot  = SoundClip::createGunshotSound(outputSampleRate_);
    auto reload   = SoundClip::createReloadSound(outputSampleRate_);
    auto dryfire  = SoundClip::createDryFireSound(outputSampleRate_);
    auto footstep = SoundClip::createFootstepSound(outputSampleRate_);
    auto impact   = SoundClip::createImpactSound(outputSampleRate_);

    resources.registerResource("weapons/m4_fire.wav", gunshot);
    resources.registerResource("weapons/m4_reload.wav", reload);
    resources.registerResource("weapons/dryfire.wav", dryfire);
    resources.registerResource("player/footstep.wav", footstep);
    resources.registerResource("weapons/impact.wav", impact);

    registerClip("weapons/m4_fire.wav", gunshot);
    registerClip("weapons/usp_fire.wav", gunshot);
    registerClip("weapons/gl_fire.wav", gunshot);
    registerClip("weapons/m4_reload.wav", reload);
    registerClip("weapons/usp_reload.wav", reload);
    registerClip("weapons/gl_reload.wav", reload);
    registerClip("weapons/dryfire.wav", dryfire);
    registerClip("player/footstep.wav", footstep);
    registerClip("weapons/impact.wav", impact);

    if (!headlessOrSoftwareMix) {
        if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
                SDL_AudioSpec desired{};
                desired.freq = static_cast<int>(outputSampleRate_);
                desired.format = AUDIO_S16SYS;
                desired.channels = 2;
                desired.samples = 1024;
                desired.callback = &AudioEngine::sdlAudioCallback;
                desired.userdata = this;

                SDL_AudioSpec obtained{};
                sdlDeviceId_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
                if (sdlDeviceId_ != 0) {
                    SDL_PauseAudioDevice(sdlDeviceId_, 0);
                }
            }
        }
    }

    initialized_ = true;
    core::Logger::instance().info(
        "AudioEngine",
        std::string("Initialized 3D Spatial AudioEngine (backend=") +
            (sdlDeviceId_ != 0 ? "SDL2 Hardware/Driver" : "Software Spatial Mixer") +
            ", clips=" + std::to_string(clips_.size()) + ")");
    return true;
}

void AudioEngine::shutdown() noexcept {
    if (sdlDeviceId_ != 0) {
        SDL_CloseAudioDevice(sdlDeviceId_);
        sdlDeviceId_ = 0;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    activeVoices_.clear();
    clips_.clear();
    initialized_ = false;
}

void AudioEngine::registerClip(const std::string& id, std::shared_ptr<SoundClip> clip) {
    if (!clip) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    clips_[id] = std::move(clip);
}

std::shared_ptr<SoundClip> AudioEngine::findClip(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = clips_.find(id);
    return (it != clips_.end()) ? it->second : nullptr;
}

void AudioEngine::setMasterVolume(float volume) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    masterVolume_ = std::clamp(volume, 0.0f, 1.0f);
}

void AudioEngine::setListenerTransform(
    const glm::vec3& position,
    const glm::vec3& forward,
    const glm::vec3& right,
    const glm::vec3& up) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    listenerPos_ = position;
    listenerForward_ = forward;
    listenerRight_ = right;
    listenerUp_ = up;
}

bool AudioEngine::playSound2D(const std::string& clipId, float volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = clips_.find(clipId);
    if (it == clips_.end() || !it->second) {
        return false;
    }
    activeVoices_.push_back(ActiveSoundVoice{
        .clip = it->second,
        .sampleCursor = 0,
        .baseVolume = std::clamp(volume, 0.0f, 2.0f),
        .spatial3D = false
    });
    ++totalSoundsPlayed_;
    return true;
}

bool AudioEngine::playSound3D(
    const std::string& clipId,
    const glm::vec3& position,
    float volume,
    float minDistance,
    float maxDistance) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = clips_.find(clipId);
    if (it == clips_.end() || !it->second) {
        return false;
    }
    activeVoices_.push_back(ActiveSoundVoice{
        .clip = it->second,
        .sampleCursor = 0,
        .baseVolume = std::clamp(volume, 0.0f, 2.0f),
        .spatial3D = true,
        .worldPosition = position,
        .minDistance = std::max(0.1f, minDistance),
        .maxDistance = std::max(minDistance + 0.1f, maxDistance)
    });
    ++totalSoundsPlayed_;
    return true;
}

void AudioEngine::mixStereoFrames(std::int16_t* outInterleavedStereo, std::size_t frameCount) noexcept {
    if (outInterleavedStereo == nullptr || frameCount == 0) {
        return;
    }
    std::memset(outInterleavedStereo, 0, frameCount * 2 * sizeof(std::int16_t));

    std::lock_guard<std::mutex> lock(mutex_);
    if (activeVoices_.empty()) {
        return;
    }

    for (std::size_t f = 0; f < frameCount; ++f) {
        float mixL = 0.0f;
        float mixR = 0.0f;

        for (auto& voice : activeVoices_) {
            if (!voice.clip || voice.sampleCursor >= voice.clip->samples().size()) {
                continue;
            }

            float gainL = voice.baseVolume * masterVolume_;
            float gainR = voice.baseVolume * masterVolume_;

            if (voice.spatial3D) {
                const glm::vec3 toSource = voice.worldPosition - listenerPos_;
                const float dist = glm::length(toSource);
                float atten = 1.0f;
                if (dist >= voice.maxDistance) {
                    atten = 0.0f;
                } else if (dist > voice.minDistance) {
                    atten = 1.0f - (dist - voice.minDistance) / (voice.maxDistance - voice.minDistance);
                }
                float pan = 0.0f; // -1 = full left, +1 = full right
                if (dist > 1e-4f) {
                    pan = std::clamp(glm::dot(toSource / dist, listenerRight_), -1.0f, 1.0f);
                }
                gainL *= atten * std::clamp(1.0f - std::max(0.0f, pan), 0.0f, 1.0f);
                gainR *= atten * std::clamp(1.0f + std::min(0.0f, pan), 0.0f, 1.0f);
            }

            const float sampleNorm =
                static_cast<float>(voice.clip->samples()[voice.sampleCursor]) / 32768.0f;
            mixL += sampleNorm * gainL;
            mixR += sampleNorm * gainR;
            voice.sampleCursor += voice.clip->channels();
        }

        const float clampedL = std::clamp(mixL, -1.0f, 1.0f);
        const float clampedR = std::clamp(mixR, -1.0f, 1.0f);
        outInterleavedStereo[f * 2 + 0] = static_cast<std::int16_t>(clampedL * 32767.0f);
        outInterleavedStereo[f * 2 + 1] = static_cast<std::int16_t>(clampedR * 32767.0f);
    }

    // Remove finished voices
    activeVoices_.erase(
        std::remove_if(
            activeVoices_.begin(),
            activeVoices_.end(),
            [](const ActiveSoundVoice& v) {
                return !v.clip || v.sampleCursor >= v.clip->samples().size();
            }),
        activeVoices_.end());
}

std::size_t AudioEngine::activeVoiceCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeVoices_.size();
}

void AudioEngine::sdlAudioCallback(void* userdata, std::uint8_t* stream, int len) noexcept {
    auto* self = static_cast<AudioEngine*>(userdata);
    if (self == nullptr || stream == nullptr || len <= 0) {
        return;
    }
    const std::size_t frameCount = static_cast<std::size_t>(len) / (2 * sizeof(std::int16_t));
    self->mixStereoFrames(reinterpret_cast<std::int16_t*>(stream), frameCount);
}

} // namespace classicfps::audio
