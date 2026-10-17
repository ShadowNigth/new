#include "engine/replay/DemoRecorder.hpp"

#include <glm/geometric.hpp>
#include <algorithm>
#include <cstring>
#include <fstream>

namespace classicfps::replay {

namespace {

template <typename T>
void writePod(std::vector<std::uint8_t>& buf, const T& val) {
    const std::size_t offset = buf.size();
    buf.resize(offset + sizeof(T));
    std::memcpy(buf.data() + offset, &val, sizeof(T));
}

template <typename T>
bool readPod(const std::vector<std::uint8_t>& buf, std::size_t& offset, T& out) {
    if (offset + sizeof(T) > buf.size()) {
        return false;
    }
    std::memcpy(&out, buf.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

void writeString(std::vector<std::uint8_t>& buf, const std::string& str) {
    const auto len = static_cast<std::uint16_t>(std::min<std::size_t>(str.size(), 65535));
    writePod(buf, len);
    if (len > 0) {
        const std::size_t offset = buf.size();
        buf.resize(offset + len);
        std::memcpy(buf.data() + offset, str.data(), len);
    }
}

bool readString(const std::vector<std::uint8_t>& buf, std::size_t& offset, std::string& out) {
    std::uint16_t len = 0;
    if (!readPod(buf, offset, len)) {
        return false;
    }
    if (offset + len > buf.size()) {
        return false;
    }
    out.assign(reinterpret_cast<const char*>(buf.data() + offset), len);
    offset += len;
    return true;
}

} // namespace

void DemoRecorder::beginRecording(
    std::string_view mapName,
    float tickRateHz,
    const glm::vec3& spawnFeetPosition,
    float spawnYawDegrees,
    float spawnPitchDegrees) {
    header_.mapName = std::string(mapName);
    header_.tickRateHz = std::max(10.0f, tickRateHz);
    header_.spawnFeetPosition = spawnFeetPosition;
    header_.spawnYawDegrees = spawnYawDegrees;
    header_.spawnPitchDegrees = spawnPitchDegrees;
    frames_.clear();
    recording_ = true;
}

void DemoRecorder::recordTick(
    const input::PlayerCommand& command,
    const glm::vec3& feetPosition,
    const glm::vec3& velocity,
    std::uint32_t eventFlags) {
    if (!recording_) {
        return;
    }
    DemoTickFrame frame{};
    frame.tickIndex = static_cast<std::uint32_t>(frames_.size());
    frame.command = command;
    frame.recordedFeetPosition = feetPosition;
    frame.recordedVelocity = velocity;
    frame.eventFlags = eventFlags;
    frames_.push_back(frame);
}

std::vector<std::uint8_t> DemoRecorder::serializeToBytes() const {
    std::vector<std::uint8_t> buf;
    buf.reserve(64 + frames_.size() * 64);

    writePod(buf, kDemoMagic);
    writePod(buf, kDemoFormatVersion);
    writeString(buf, header_.mapName);
    writePod(buf, header_.tickRateHz);
    writePod(buf, header_.spawnFeetPosition);
    writePod(buf, header_.spawnYawDegrees);
    writePod(buf, header_.spawnPitchDegrees);

    const auto totalFrames = static_cast<std::uint32_t>(frames_.size());
    writePod(buf, totalFrames);

    for (const auto& f : frames_) {
        writePod(buf, f.tickIndex);
        writePod(buf, f.command.sequence);
        writePod(buf, f.command.forwardMove);
        writePod(buf, f.command.rightMove);
        writePod(buf, f.command.viewYawDegrees);
        writePod(buf, f.command.viewPitchDegrees);

        std::uint8_t buttonMask = 0;
        if (f.command.jump) buttonMask |= (1U << 0U);
        if (f.command.crouch) buttonMask |= (1U << 1U);
        if (f.command.fireHeld) buttonMask |= (1U << 2U);
        if (f.command.fireJustPressed) buttonMask |= (1U << 3U);
        if (f.command.reload) buttonMask |= (1U << 4U);
        if (f.command.walk) buttonMask |= (1U << 5U);
        writePod(buf, buttonMask);
        writePod(buf, f.command.weaponSlotSelect);

        writePod(buf, f.recordedFeetPosition);
        writePod(buf, f.recordedVelocity);
        writePod(buf, f.eventFlags);
    }

    return buf;
}

bool DemoRecorder::saveToFile(const std::string& filePath) const {
    const auto bytes = serializeToBytes();
    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return out.good();
}

bool DemoPlayer::deserializeFromBytes(const std::vector<std::uint8_t>& bytes) {
    loaded_ = false;
    frames_.clear();
    std::size_t offset = 0;

    std::uint32_t magic = 0;
    std::uint16_t version = 0;
    if (!readPod(bytes, offset, magic) || magic != kDemoMagic) {
        return false;
    }
    if (!readPod(bytes, offset, version) || version != kDemoFormatVersion) {
        return false;
    }
    if (!readString(bytes, offset, header_.mapName) ||
        !readPod(bytes, offset, header_.tickRateHz) ||
        !readPod(bytes, offset, header_.spawnFeetPosition) ||
        !readPod(bytes, offset, header_.spawnYawDegrees) ||
        !readPod(bytes, offset, header_.spawnPitchDegrees)) {
        return false;
    }

    std::uint32_t totalFrames = 0;
    if (!readPod(bytes, offset, totalFrames) || totalFrames > 1000000U) {
        return false;
    }

    frames_.reserve(totalFrames);
    for (std::uint32_t i = 0; i < totalFrames; ++i) {
        DemoTickFrame f{};
        std::uint8_t buttonMask = 0;
        if (!readPod(bytes, offset, f.tickIndex) ||
            !readPod(bytes, offset, f.command.sequence) ||
            !readPod(bytes, offset, f.command.forwardMove) ||
            !readPod(bytes, offset, f.command.rightMove) ||
            !readPod(bytes, offset, f.command.viewYawDegrees) ||
            !readPod(bytes, offset, f.command.viewPitchDegrees) ||
            !readPod(bytes, offset, buttonMask) ||
            !readPod(bytes, offset, f.command.weaponSlotSelect) ||
            !readPod(bytes, offset, f.recordedFeetPosition) ||
            !readPod(bytes, offset, f.recordedVelocity) ||
            !readPod(bytes, offset, f.eventFlags)) {
            return false;
        }
        f.command.jump = (buttonMask & (1U << 0U)) != 0U;
        f.command.crouch = (buttonMask & (1U << 1U)) != 0U;
        f.command.fireHeld = (buttonMask & (1U << 2U)) != 0U;
        f.command.fireJustPressed = (buttonMask & (1U << 3U)) != 0U;
        f.command.reload = (buttonMask & (1U << 4U)) != 0U;
        f.command.walk = (buttonMask & (1U << 5U)) != 0U;
        frames_.push_back(f);
    }

    resetPlayback();
    loaded_ = true;
    return true;
}

bool DemoPlayer::loadFromFile(const std::string& filePath) {
    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        return false;
    }
    const auto sz = in.tellg();
    if (sz <= 0) {
        return false;
    }
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(sz));
    if (!in.read(reinterpret_cast<char*>(bytes.data()), sz)) {
        return false;
    }
    return deserializeFromBytes(bytes);
}

void DemoPlayer::resetPlayback() noexcept {
    currentTick_ = 0;
    timeAccumulator_ = 0.0f;
}

void DemoPlayer::seekToTick(std::size_t tickIndex) noexcept {
    currentTick_ = std::min(tickIndex, frames_.size());
    timeAccumulator_ = 0.0f;
}

void DemoPlayer::setPlaybackSpeed(float speedMultiplier) noexcept {
    playbackSpeed_ = std::clamp(speedMultiplier, 0.1f, 8.0f);
}

const DemoTickFrame* DemoPlayer::stepPlayback(float dt) noexcept {
    if (!loaded_ || currentTick_ >= frames_.size() || dt <= 0.0f) {
        return nullptr;
    }

    const float tickInterval = 1.0f / std::max(10.0f, header_.tickRateHz);
    timeAccumulator_ += dt * playbackSpeed_;

    const DemoTickFrame* lastAdvanced = nullptr;
    while (timeAccumulator_ >= tickInterval && currentTick_ < frames_.size()) {
        timeAccumulator_ -= tickInterval;
        lastAdvanced = &frames_[currentTick_++];
    }
    return lastAdvanced;
}

DeterminismReport DemoPlayer::verifyDeterminism(
    const physics::CollisionWorld& world,
    float maxAllowedErrorMeters) const {
    DeterminismReport report{};
    if (!loaded_) {
        return report;
    }

    physics::FPSController simController;
    simController.spawn(
        header_.spawnFeetPosition,
        header_.spawnYawDegrees,
        header_.spawnPitchDegrees);

    const float dt = 1.0f / std::max(10.0f, header_.tickRateHz);
    float maxErr = 0.0f;

    for (const auto& f : frames_) {
        simController.simulateStep(f.command, world, dt);
        const float err = glm::length(simController.state().feetPosition - f.recordedFeetPosition);
        maxErr = std::max(maxErr, err);
        ++report.ticksVerified;
    }

    report.maxPositionErrorMeters = maxErr;
    report.deterministic = (maxErr <= maxAllowedErrorMeters);
    return report;
}

} // namespace classicfps::replay
