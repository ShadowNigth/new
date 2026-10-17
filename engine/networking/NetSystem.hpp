#pragma once

#include "engine/input/InputManager.hpp"
#include "engine/physics/CollisionWorld.hpp"
#include "engine/physics/FPSController.hpp"

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace classicfps::networking {

constexpr std::uint32_t kNetProtocolMagic = 0x50465343U; // "CFSP"
constexpr std::uint16_t kNetProtocolVersion = 1U;

enum class NetPacketType : std::uint8_t {
    ConnectionRequest = 1,
    ConnectionAccepted = 2,
    ConnectionDisconnect = 3,
    ClientUserCommand = 4,
    ServerWorldSnapshot = 5,
    WeaponFireEvent = 6,
    Ping = 7,
    Pong = 8
};

#pragma pack(push, 1)
struct NetPacketHeader {
    std::uint32_t magic{kNetProtocolMagic};
    std::uint16_t version{kNetProtocolVersion};
    std::uint8_t type{0};
    std::uint32_t sequence{0};
    std::uint32_t ackSequence{0};
    std::uint32_t ackBitfield{0}; // 32-bit sliding window of prior received packets
};

struct ClientUserCmdPayload {
    std::uint32_t clientId{0};
    std::uint32_t cmdSequence{0};
    float forwardMove{0.0f};
    float rightMove{0.0f};
    float viewYawDegrees{-90.0f};
    float viewPitchDegrees{0.0f};
    std::uint8_t buttonsMask{0}; // bit0=jump, bit1=crouch, bit2=walk, bit3=fire, bit4=reload
    float fixedDt{0.01f};
};

struct ServerPlayerSnapshot {
    std::uint32_t clientId{0};
    std::uint32_t lastAckedCmdSequence{0};
    float feetX{0.0f};
    float feetY{0.0f};
    float feetZ{0.0f};
    float velX{0.0f};
    float velY{0.0f};
    float velZ{0.0f};
    float yawDegrees{-90.0f};
    float pitchDegrees{0.0f};
    std::uint8_t flags{0}; // bit0=onGround, bit1=crouching
    std::uint8_t health{100};
};
#pragma pack(pop)

struct NetEndpoint {
    std::string address{"127.0.0.1"};
    std::uint16_t port{27015};
};

class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool openAndBind(std::uint16_t port = 0, const std::string& bindAddress = "127.0.0.1");
    void close() noexcept;

    [[nodiscard]] bool isOpen() const noexcept { return fd_ >= 0; }
    [[nodiscard]] std::uint16_t boundPort() const noexcept { return boundPort_; }

    bool sendPacket(const NetEndpoint& target, const void* data, std::size_t size) noexcept;
    int receivePacket(void* outBuffer, std::size_t maxLen, NetEndpoint& outSender) noexcept;

private:
    int fd_{-1};
    std::uint16_t boundPort_{0};
};

class NetServer {
public:
    NetServer() = default;
    ~NetServer() { stop(); }

    bool start(std::uint16_t port, const physics::CollisionWorld* world);
    void stop() noexcept;

    void pollAndSimulate(float fixedDt);

    [[nodiscard]] bool isRunning() const noexcept { return socket_.isOpen(); }
    [[nodiscard]] std::uint16_t port() const noexcept { return socket_.boundPort(); }
    [[nodiscard]] std::size_t connectedClientCount() const noexcept { return clients_.size(); }

    // Allows testing server discrepancy injection (e.g. knockback or teleport) to verify client reconciliation
    void forceClientPosition(std::uint32_t clientId, const glm::vec3& newFeetPosition);

private:
    struct ClientSlot {
        std::uint32_t clientId{0};
        NetEndpoint endpoint{};
        std::uint32_t lastCmdSequence{0};
        physics::FPSController controller{};
        std::uint8_t health{100};
    };

    UdpSocket socket_;
    const physics::CollisionWorld* world_{nullptr};
    std::vector<ClientSlot> clients_;
    std::uint32_t nextClientId_{1};
    std::uint32_t serverSequence_{1};
};

class NetClient {
public:
    NetClient() = default;
    ~NetClient() { disconnect(); }

    bool connectToServer(
        const std::string& serverAddress,
        std::uint16_t serverPort,
        const physics::CollisionWorld* world,
        const glm::vec3& initialSpawnFeet = glm::vec3(0.0f));

    void disconnect() noexcept;

    // Sends command to server AND immediately predicts local movement
    void sendAndPredictCommand(const input::PlayerCommand& cmd, float fixedDt);

    // Polls incoming snapshots from server and runs reconciliation if needed
    void pollNetwork();

    [[nodiscard]] bool isConnected() const noexcept { return connected_; }
    [[nodiscard]] std::uint32_t clientId() const noexcept { return clientId_; }
    [[nodiscard]] const physics::FPSController& predictedController() const noexcept {
        return localController_;
    }
    [[nodiscard]] std::uint32_t reconciliationCount() const noexcept {
        return reconciliationCount_;
    }
    [[nodiscard]] std::uint32_t snapshotsReceived() const noexcept {
        return snapshotsReceived_;
    }

private:
    struct PredictedStep {
        std::uint32_t sequence{0};
        input::PlayerCommand cmd{};
        float fixedDt{0.01f};
        glm::vec3 predictedFeetPos{0.0f};
    };

    void reconcileWithSnapshot(const ServerPlayerSnapshot& snap);

    UdpSocket socket_;
    NetEndpoint serverEndpoint_{};
    const physics::CollisionWorld* world_{nullptr};
    bool connected_{false};
    std::uint32_t clientId_{0};
    std::uint32_t packetSequence_{1};
    physics::FPSController localController_{};
    std::vector<PredictedStep> predictionHistory_;
    std::uint32_t reconciliationCount_{0};
    std::uint32_t snapshotsReceived_{0};
};

// Smoothly interpolates remote player positions between received server snapshots
class SnapshotInterpolator {
public:
    struct TimedPose {
        double timestampSeconds{0.0};
        glm::vec3 feetPosition{0.0f};
        float yawDegrees{-90.0f};
        float pitchDegrees{0.0f};
    };

    void pushSnapshot(double timestampSeconds, const glm::vec3& feetPos, float yawDeg, float pitchDeg);
    [[nodiscard]] TimedPose sampleAt(double renderTimestampSeconds) const noexcept;
    [[nodiscard]] std::size_t bufferedCount() const noexcept { return buffer_.size(); }

private:
    std::vector<TimedPose> buffer_;
};

// Server-side lag compensation history buffer for rewinding hitboxes during hitscan verification
class LagCompensator {
public:
    struct HistoricalHitbox {
        double serverTimeSeconds{0.0};
        std::uint32_t playerId{0};
        glm::vec3 center{0.0f};
        glm::vec3 halfExtents{0.35f, 0.90f, 0.35f};
    };

    void recordPlayerState(
        double serverTimeSeconds,
        std::uint32_t playerId,
        const glm::vec3& feetPosition,
        float height = 1.80f);

    // Rewinds to targetTimeSeconds (e.g. serverTime - rtt/2) and tests ray intersection
    [[nodiscard]] bool traceRewoundRay(
        double targetTimeSeconds,
        std::uint32_t targetPlayerId,
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDir,
        float maxRange,
        float& outHitDistance) const noexcept;

private:
    std::vector<HistoricalHitbox> history_;
};

} // namespace classicfps::networking
