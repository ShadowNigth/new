#include "engine/networking/NetSystem.hpp"
#include "engine/core/Logger.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace classicfps::networking {

UdpSocket::~UdpSocket() {
    close();
}

bool UdpSocket::openAndBind(std::uint16_t port, const std::string& bindAddress) {
    close();

    fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd_ < 0) {
        return false;
    }

    int reuse = 1;
    (void)::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    const int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        close();
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, bindAddress.c_str(), &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    if (::bind(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        close();
        return false;
    }

    sockaddr_in boundAddr{};
    socklen_t addrLen = sizeof(boundAddr);
    if (::getsockname(fd_, reinterpret_cast<sockaddr*>(&boundAddr), &addrLen) == 0) {
        boundPort_ = ntohs(boundAddr.sin_port);
    } else {
        boundPort_ = port;
    }

    return true;
}

void UdpSocket::close() noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    boundPort_ = 0;
}

bool UdpSocket::sendPacket(const NetEndpoint& target, const void* data, std::size_t size) noexcept {
    if (fd_ < 0 || data == nullptr || size == 0) {
        return false;
    }
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(target.port);
    if (::inet_pton(AF_INET, target.address.c_str(), &dest.sin_addr) != 1) {
        return false;
    }
    const ssize_t sent = ::sendto(
        fd_,
        data,
        size,
        0,
        reinterpret_cast<const sockaddr*>(&dest),
        sizeof(dest));
    return sent == static_cast<ssize_t>(size);
}

int UdpSocket::receivePacket(void* outBuffer, std::size_t maxLen, NetEndpoint& outSender) noexcept {
    if (fd_ < 0 || outBuffer == nullptr || maxLen == 0) {
        return -1;
    }
    sockaddr_in src{};
    socklen_t srcLen = sizeof(src);
    const ssize_t recvd = ::recvfrom(
        fd_,
        outBuffer,
        maxLen,
        0,
        reinterpret_cast<sockaddr*>(&src),
        &srcLen);
    if (recvd <= 0) {
        return -1;
    }
    char ipStr[INET_ADDRSTRLEN]{};
    if (::inet_ntop(AF_INET, &src.sin_addr, ipStr, sizeof(ipStr)) != nullptr) {
        outSender.address = ipStr;
    }
    outSender.port = ntohs(src.sin_port);
    return static_cast<int>(recvd);
}

bool NetServer::start(std::uint16_t port, const physics::CollisionWorld* world) {
    stop();
    world_ = world;
    if (!socket_.openAndBind(port, "127.0.0.1")) {
        return false;
    }
    core::Logger::instance().info(
        "NetServer",
        "Started authoritative UDP server on 127.0.0.1:" + std::to_string(socket_.boundPort()));
    return true;
}

void NetServer::stop() noexcept {
    socket_.close();
    clients_.clear();
    world_ = nullptr;
}

void NetServer::forceClientPosition(std::uint32_t clientId, const glm::vec3& newFeetPosition) {
    for (auto& c : clients_) {
        if (c.clientId == clientId) {
            c.controller.state().feetPosition = newFeetPosition;
            c.controller.state().prevFeetPosition = newFeetPosition;
            return;
        }
    }
}

void NetServer::pollAndSimulate(float fixedDt) {
    if (!socket_.isOpen()) {
        return;
    }

    std::uint8_t buf[512]{};
    NetEndpoint sender{};

    while (true) {
        const int bytes = socket_.receivePacket(buf, sizeof(buf), sender);
        if (bytes < static_cast<int>(sizeof(NetPacketHeader))) {
            break;
        }

        NetPacketHeader hdr{};
        std::memcpy(&hdr, buf, sizeof(NetPacketHeader));
        if (hdr.magic != kNetProtocolMagic || hdr.version != kNetProtocolVersion) {
            continue;
        }

        const auto pktType = static_cast<NetPacketType>(hdr.type);
        if (pktType == NetPacketType::ConnectionRequest) {
            glm::vec3 initialSpawn{0.0f};
            if (bytes >= static_cast<int>(sizeof(NetPacketHeader) + sizeof(glm::vec3))) {
                std::memcpy(&initialSpawn, buf + sizeof(NetPacketHeader), sizeof(glm::vec3));
            }

            ClientSlot slot{};
            slot.clientId = nextClientId_++;
            slot.endpoint = sender;
            slot.controller.spawn(initialSpawn, -90.0f, 0.0f);
            clients_.push_back(slot);

            // Reply with ConnectionAccepted + assigned clientId
            std::uint8_t reply[sizeof(NetPacketHeader) + sizeof(std::uint32_t)]{};
            NetPacketHeader repHdr{
                .magic = kNetProtocolMagic,
                .version = kNetProtocolVersion,
                .type = static_cast<std::uint8_t>(NetPacketType::ConnectionAccepted),
                .sequence = serverSequence_++
            };
            std::memcpy(reply, &repHdr, sizeof(NetPacketHeader));
            std::memcpy(reply + sizeof(NetPacketHeader), &slot.clientId, sizeof(std::uint32_t));
            (void)socket_.sendPacket(sender, reply, sizeof(reply));
        } else if (pktType == NetPacketType::ClientUserCommand) {
            if (bytes < static_cast<int>(sizeof(NetPacketHeader) + sizeof(ClientUserCmdPayload))) {
                continue;
            }
            ClientUserCmdPayload payload{};
            std::memcpy(&payload, buf + sizeof(NetPacketHeader), sizeof(ClientUserCmdPayload));

            for (auto& c : clients_) {
                if (c.clientId == payload.clientId && payload.cmdSequence > c.lastCmdSequence) {
                    c.lastCmdSequence = payload.cmdSequence;
                    input::PlayerCommand cmd{};
                    cmd.sequence = payload.cmdSequence;
                    cmd.forwardMove = payload.forwardMove;
                    cmd.rightMove = payload.rightMove;
                    cmd.viewYawDegrees = payload.viewYawDegrees;
                    cmd.viewPitchDegrees = payload.viewPitchDegrees;
                    cmd.jump = (payload.buttonsMask & 0x01U) != 0U;
                    cmd.crouch = (payload.buttonsMask & 0x02U) != 0U;
                    cmd.walk = (payload.buttonsMask & 0x04U) != 0U;

                    if (world_ != nullptr) {
                        c.controller.simulateStep(cmd, *world_, payload.fixedDt > 0.0f ? payload.fixedDt : fixedDt);
                    }
                    break;
                }
            }
        }
    }

    // Broadcast authoritative snapshots to all connected clients
    for (const auto& c : clients_) {
        const auto& st = c.controller.state();
        ServerPlayerSnapshot snap{};
        snap.clientId = c.clientId;
        snap.lastAckedCmdSequence = c.lastCmdSequence;
        snap.feetX = st.feetPosition.x;
        snap.feetY = st.feetPosition.y;
        snap.feetZ = st.feetPosition.z;
        snap.velX = st.velocity.x;
        snap.velY = st.velocity.y;
        snap.velZ = st.velocity.z;
        snap.yawDegrees = st.yawDegrees;
        snap.pitchDegrees = st.pitchDegrees;
        snap.flags = (st.onGround ? 0x01U : 0x00U) | (st.crouching ? 0x02U : 0x00U);
        snap.health = c.health;

        std::uint8_t outBuf[sizeof(NetPacketHeader) + sizeof(ServerPlayerSnapshot)]{};
        NetPacketHeader hdr{
            .magic = kNetProtocolMagic,
            .version = kNetProtocolVersion,
            .type = static_cast<std::uint8_t>(NetPacketType::ServerWorldSnapshot),
            .sequence = serverSequence_++
        };
        std::memcpy(outBuf, &hdr, sizeof(NetPacketHeader));
        std::memcpy(outBuf + sizeof(NetPacketHeader), &snap, sizeof(ServerPlayerSnapshot));
        (void)socket_.sendPacket(c.endpoint, outBuf, sizeof(outBuf));
    }
}

bool NetClient::connectToServer(
    const std::string& serverAddress,
    std::uint16_t serverPort,
    const physics::CollisionWorld* world,
    const glm::vec3& initialSpawnFeet) {
    disconnect();
    world_ = world;
    if (!socket_.openAndBind(0, "127.0.0.1")) {
        return false;
    }

    serverEndpoint_.address = serverAddress;
    serverEndpoint_.port = serverPort;
    localController_.spawn(initialSpawnFeet, -90.0f, 0.0f);

    std::uint8_t req[sizeof(NetPacketHeader) + sizeof(glm::vec3)]{};
    NetPacketHeader hdr{
        .magic = kNetProtocolMagic,
        .version = kNetProtocolVersion,
        .type = static_cast<std::uint8_t>(NetPacketType::ConnectionRequest),
        .sequence = packetSequence_++
    };
    std::memcpy(req, &hdr, sizeof(NetPacketHeader));
    std::memcpy(req + sizeof(NetPacketHeader), &initialSpawnFeet, sizeof(glm::vec3));
    return socket_.sendPacket(serverEndpoint_, req, sizeof(req));
}

void NetClient::disconnect() noexcept {
    socket_.close();
    connected_ = false;
    clientId_ = 0;
    predictionHistory_.clear();
}

void NetClient::sendAndPredictCommand(const input::PlayerCommand& cmd, float fixedDt) {
    if (!socket_.isOpen() || world_ == nullptr) {
        return;
    }

    // 1. Predict movement locally immediately
    localController_.simulateStep(cmd, *world_, fixedDt);

    predictionHistory_.push_back(PredictedStep{
        .sequence = cmd.sequence,
        .cmd = cmd,
        .fixedDt = fixedDt,
        .predictedFeetPos = localController_.state().feetPosition
    });
    if (predictionHistory_.size() > 128) {
        predictionHistory_.erase(predictionHistory_.begin());
    }

    // 2. Send command packet to server
    ClientUserCmdPayload payload{};
    payload.clientId = clientId_;
    payload.cmdSequence = cmd.sequence;
    payload.forwardMove = cmd.forwardMove;
    payload.rightMove = cmd.rightMove;
    payload.viewYawDegrees = cmd.viewYawDegrees;
    payload.viewPitchDegrees = cmd.viewPitchDegrees;
    payload.buttonsMask =
        (cmd.jump ? 0x01U : 0x00U) |
        (cmd.crouch ? 0x02U : 0x00U) |
        (cmd.walk ? 0x04U : 0x00U) |
        (cmd.fireHeld ? 0x08U : 0x00U) |
        (cmd.reload ? 0x10U : 0x00U);
    payload.fixedDt = fixedDt;

    std::uint8_t outBuf[sizeof(NetPacketHeader) + sizeof(ClientUserCmdPayload)]{};
    NetPacketHeader hdr{
        .magic = kNetProtocolMagic,
        .version = kNetProtocolVersion,
        .type = static_cast<std::uint8_t>(NetPacketType::ClientUserCommand),
        .sequence = packetSequence_++
    };
    std::memcpy(outBuf, &hdr, sizeof(NetPacketHeader));
    std::memcpy(outBuf + sizeof(NetPacketHeader), &payload, sizeof(ClientUserCmdPayload));
    (void)socket_.sendPacket(serverEndpoint_, outBuf, sizeof(outBuf));
}

void NetClient::pollNetwork() {
    if (!socket_.isOpen()) {
        return;
    }

    std::uint8_t buf[512]{};
    NetEndpoint sender{};

    while (true) {
        const int bytes = socket_.receivePacket(buf, sizeof(buf), sender);
        if (bytes < static_cast<int>(sizeof(NetPacketHeader))) {
            break;
        }

        NetPacketHeader hdr{};
        std::memcpy(&hdr, buf, sizeof(NetPacketHeader));
        if (hdr.magic != kNetProtocolMagic || hdr.version != kNetProtocolVersion) {
            continue;
        }

        const auto pktType = static_cast<NetPacketType>(hdr.type);
        if (pktType == NetPacketType::ConnectionAccepted) {
            if (bytes >= static_cast<int>(sizeof(NetPacketHeader) + sizeof(std::uint32_t))) {
                std::memcpy(&clientId_, buf + sizeof(NetPacketHeader), sizeof(std::uint32_t));
                connected_ = true;
            }
        } else if (pktType == NetPacketType::ServerWorldSnapshot) {
            if (bytes >= static_cast<int>(sizeof(NetPacketHeader) + sizeof(ServerPlayerSnapshot))) {
                ServerPlayerSnapshot snap{};
                std::memcpy(&snap, buf + sizeof(NetPacketHeader), sizeof(ServerPlayerSnapshot));
                if (snap.clientId == clientId_) {
                    ++snapshotsReceived_;
                    reconcileWithSnapshot(snap);
                }
            }
        }
    }
}

void NetClient::reconcileWithSnapshot(const ServerPlayerSnapshot& snap) {
    if (world_ == nullptr) {
        return;
    }

    const glm::vec3 serverFeetPos(snap.feetX, snap.feetY, snap.feetZ);

    // Discard acknowledged commands older than snap.lastAckedCmdSequence
    while (!predictionHistory_.empty() &&
           predictionHistory_.front().sequence < snap.lastAckedCmdSequence) {
        predictionHistory_.erase(predictionHistory_.begin());
    }

    if (predictionHistory_.empty() ||
        predictionHistory_.front().sequence != snap.lastAckedCmdSequence) {
        return;
    }

    const glm::vec3 predictedAtAck = predictionHistory_.front().predictedFeetPos;
    predictionHistory_.erase(predictionHistory_.begin());

    const float errorDist = glm::distance(serverFeetPos, predictedAtAck);
    constexpr float kReconcileThreshold = 0.02f;

    if (errorDist > kReconcileThreshold) {
        // Rewind to authoritative server state and replay all unacknowledged commands!
        ++reconciliationCount_;
        auto& st = localController_.state();
        st.feetPosition = serverFeetPos;
        st.prevFeetPosition = serverFeetPos;
        st.velocity = glm::vec3(snap.velX, snap.velY, snap.velZ);
        st.yawDegrees = snap.yawDegrees;
        st.pitchDegrees = snap.pitchDegrees;
        st.onGround = (snap.flags & 0x01U) != 0U;
        st.crouching = (snap.flags & 0x02U) != 0U;

        for (auto& unacked : predictionHistory_) {
            localController_.simulateStep(unacked.cmd, *world_, unacked.fixedDt);
            unacked.predictedFeetPos = localController_.state().feetPosition;
        }
    }
}

void SnapshotInterpolator::pushSnapshot(
    double timestampSeconds,
    const glm::vec3& feetPos,
    float yawDeg,
    float pitchDeg) {
    buffer_.push_back(TimedPose{
        .timestampSeconds = timestampSeconds,
        .feetPosition = feetPos,
        .yawDegrees = yawDeg,
        .pitchDegrees = pitchDeg
    });
    if (buffer_.size() > 64) {
        buffer_.erase(buffer_.begin());
    }
}

SnapshotInterpolator::TimedPose SnapshotInterpolator::sampleAt(double renderTimestampSeconds) const noexcept {
    if (buffer_.empty()) {
        return TimedPose{};
    }
    if (buffer_.size() == 1 || renderTimestampSeconds <= buffer_.front().timestampSeconds) {
        return buffer_.front();
    }
    if (renderTimestampSeconds >= buffer_.back().timestampSeconds) {
        return buffer_.back();
    }

    for (std::size_t i = 0; i + 1 < buffer_.size(); ++i) {
        const TimedPose& a = buffer_[i];
        const TimedPose& b = buffer_[i + 1];
        if (renderTimestampSeconds >= a.timestampSeconds &&
            renderTimestampSeconds <= b.timestampSeconds) {
            const double span = b.timestampSeconds - a.timestampSeconds;
            const float alpha = (span > 1e-6)
                ? static_cast<float>((renderTimestampSeconds - a.timestampSeconds) / span)
                : 0.0f;
            TimedPose out{};
            out.timestampSeconds = renderTimestampSeconds;
            out.feetPosition = glm::mix(a.feetPosition, b.feetPosition, alpha);
            out.yawDegrees = a.yawDegrees + (b.yawDegrees - a.yawDegrees) * alpha;
            out.pitchDegrees = a.pitchDegrees + (b.pitchDegrees - a.pitchDegrees) * alpha;
            return out;
        }
    }
    return buffer_.back();
}

void LagCompensator::recordPlayerState(
    double serverTimeSeconds,
    std::uint32_t playerId,
    const glm::vec3& feetPosition,
    float height) {
    history_.push_back(HistoricalHitbox{
        .serverTimeSeconds = serverTimeSeconds,
        .playerId = playerId,
        .center = feetPosition + glm::vec3(0.0f, height * 0.5f, 0.0f),
        .halfExtents = glm::vec3(0.35f, height * 0.5f, 0.35f)
    });
    if (history_.size() > 256) {
        history_.erase(history_.begin());
    }
}

bool LagCompensator::traceRewoundRay(
    double targetTimeSeconds,
    std::uint32_t targetPlayerId,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    float maxRange,
    float& outHitDistance) const noexcept {
    const HistoricalHitbox* best = nullptr;
    double bestTimeDiff = 1e9;

    for (const auto& h : history_) {
        if (h.playerId != targetPlayerId) {
            continue;
        }
        const double diff = std::abs(h.serverTimeSeconds - targetTimeSeconds);
        if (diff < bestTimeDiff) {
            bestTimeDiff = diff;
            best = &h;
        }
    }

    if (best == nullptr) {
        return false;
    }

    // Ray vs AABB slab test on rewound hitbox
    const glm::vec3 bMin = best->center - best->halfExtents;
    const glm::vec3 bMax = best->center + best->halfExtents;
    float tMin = 0.0f;
    float tMax = maxRange;

    for (int axis = 0; axis < 3; ++axis) {
        const float o = rayOrigin[axis];
        const float d = rayDir[axis];
        if (std::abs(d) < 1e-6f) {
            if (o < bMin[axis] || o > bMax[axis]) {
                return false;
            }
        } else {
            const float invD = 1.0f / d;
            float t1 = (bMin[axis] - o) * invD;
            float t2 = (bMax[axis] - o) * invD;
            if (t1 > t2) {
                std::swap(t1, t2);
            }
            if (t1 > tMin) tMin = t1;
            if (t2 < tMax) tMax = t2;
            if (tMin > tMax) {
                return false;
            }
        }
    }

    outHitDistance = tMin;
    return (tMin >= 0.0f && tMin <= maxRange);
}

} // namespace classicfps::networking
