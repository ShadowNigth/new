#include "engine/ai/BotAI.hpp"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>

namespace classicfps::ai {

void NavGraph::clear() {
    nodes_.clear();
}

int NavGraph::addWaypoint(const glm::vec3& position, WaypointTag tag) {
    const int id = static_cast<int>(nodes_.size()) + 1;
    nodes_.push_back(WaypointNode{
        .id = id,
        .position = position,
        .tag = tag,
        .neighbors = {}
    });
    return id;
}

void NavGraph::connectWaypoints(int idA, int idB, bool bidirectional) {
    if (idA == idB) {
        return;
    }
    for (auto& n : nodes_) {
        if (n.id == idA) {
            if (std::find(n.neighbors.begin(), n.neighbors.end(), idB) == n.neighbors.end()) {
                n.neighbors.push_back(idB);
            }
        } else if (bidirectional && n.id == idB) {
            if (std::find(n.neighbors.begin(), n.neighbors.end(), idA) == n.neighbors.end()) {
                n.neighbors.push_back(idA);
            }
        }
    }
}

void NavGraph::autoLinkByVisibility(
    const physics::CollisionWorld& world,
    float maxLinkDistance) {
    const glm::vec3 chestOffset{0.0f, 1.0f, 0.0f};
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        for (std::size_t j = i + 1; j < nodes_.size(); ++j) {
            const glm::vec3 a = nodes_[i].position + chestOffset;
            const glm::vec3 b = nodes_[j].position + chestOffset;
            const glm::vec3 delta = b - a;
            const float dist = glm::length(delta);
            if (dist <= 0.05f || dist > maxLinkDistance) {
                continue;
            }
            const auto hit = world.raycast(a, delta / dist, dist - 0.05f);
            if (!hit.hit) {
                connectWaypoints(nodes_[i].id, nodes_[j].id, true);
            }
        }
    }
}

void NavGraph::buildDefaultArenaGraph(const physics::CollisionWorld& world) {
    clear();
    const int nSpawnCT = addWaypoint({0.0f, 0.0f, 7.5f}, WaypointTag::SpawnCT);
    const int nSouthMid = addWaypoint({0.0f, 0.0f, 3.8f}, WaypointTag::MidPatrol);
    const int nWestFlank = addWaypoint({-6.5f, 0.0f, 2.5f}, WaypointTag::General);
    const int nEastFlank = addWaypoint({6.5f, 0.0f, 2.5f}, WaypointTag::General);
    const int nWestNorth = addWaypoint({-6.5f, 0.0f, -3.5f}, WaypointTag::Cover);
    const int nEastNorth = addWaypoint({6.5f, 0.0f, -3.5f}, WaypointTag::Cover);
    const int nBombsiteA = addWaypoint({0.0f, 0.0f, -6.2f}, WaypointTag::BombsiteA);
    const int nSpawnT = addWaypoint({-7.5f, 0.0f, -7.8f}, WaypointTag::SpawnT);

    connectWaypoints(nSpawnCT, nSouthMid);
    connectWaypoints(nSouthMid, nWestFlank);
    connectWaypoints(nSouthMid, nEastFlank);
    connectWaypoints(nWestFlank, nWestNorth);
    connectWaypoints(nEastFlank, nEastNorth);
    connectWaypoints(nWestNorth, nBombsiteA);
    connectWaypoints(nEastNorth, nBombsiteA);
    connectWaypoints(nWestNorth, nSpawnT);
    connectWaypoints(nBombsiteA, nSpawnT);

    autoLinkByVisibility(world, 9.5f);
}

const WaypointNode* NavGraph::getNode(int id) const noexcept {
    for (const auto& n : nodes_) {
        if (n.id == id) {
            return &n;
        }
    }
    return nullptr;
}

int NavGraph::findNearestWaypoint(const glm::vec3& pos) const noexcept {
    int bestId = -1;
    float bestDistSq = std::numeric_limits<float>::max();
    for (const auto& n : nodes_) {
        const glm::vec3 d = n.position - pos;
        const float distSq = glm::dot(d, d);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = n.id;
        }
    }
    return bestId;
}

int NavGraph::findWaypointByTag(WaypointTag tag) const noexcept {
    for (const auto& n : nodes_) {
        if (n.tag == tag) {
            return n.id;
        }
    }
    return -1;
}

std::vector<glm::vec3> NavGraph::findPathAStar(int startNodeId, int goalNodeId) const {
    const WaypointNode* startNode = getNode(startNodeId);
    const WaypointNode* goalNode = getNode(goalNodeId);
    if (startNode == nullptr || goalNode == nullptr) {
        return {};
    }
    if (startNodeId == goalNodeId) {
        return {startNode->position};
    }

    struct QueueEntry {
        int id{0};
        float fScore{0.0f};
        bool operator>(const QueueEntry& other) const noexcept {
            return fScore > other.fScore;
        }
    };

    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> openSet;
    std::unordered_map<int, int> cameFrom;
    std::unordered_map<int, float> gScore;

    for (const auto& n : nodes_) {
        gScore[n.id] = std::numeric_limits<float>::infinity();
    }

    gScore[startNodeId] = 0.0f;
    openSet.push(QueueEntry{startNodeId, glm::length(goalNode->position - startNode->position)});

    while (!openSet.empty()) {
        const int currentId = openSet.top().id;
        openSet.pop();

        if (currentId == goalNodeId) {
            std::vector<glm::vec3> path;
            int trace = goalNodeId;
            while (true) {
                const WaypointNode* wn = getNode(trace);
                if (wn != nullptr) {
                    path.push_back(wn->position);
                }
                auto it = cameFrom.find(trace);
                if (it == cameFrom.end()) {
                    break;
                }
                trace = it->second;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        const WaypointNode* current = getNode(currentId);
        if (current == nullptr) {
            continue;
        }

        for (const int neighborId : current->neighbors) {
            const WaypointNode* neighbor = getNode(neighborId);
            if (neighbor == nullptr) {
                continue;
            }
            const float tentativeG =
                gScore[currentId] + glm::length(neighbor->position - current->position);
            if (tentativeG < gScore[neighborId]) {
                cameFrom[neighborId] = currentId;
                gScore[neighborId] = tentativeG;
                const float h = glm::length(goalNode->position - neighbor->position);
                openSet.push(QueueEntry{neighborId, tentativeG + h});
            }
        }
    }

    return {};
}

BotAgent::BotAgent(const BotConfig& cfg) : config_(cfg) {}

void BotAgent::spawn(const glm::vec3& feetPosition, float yawDegrees) {
    yawDegrees_ = yawDegrees;
    pitchDegrees_ = 0.0f;
    health_ = 100;
    state_ = BotState::Patrol;
    enemyReactionTimer_ = 0.0f;
    currentPath_.clear();
    pathIndex_ = 0;
    patrolGoalNodeId_ = -1;
    controller_.spawn(feetPosition, yawDegrees, 0.0f);
}

bool BotAgent::canSeePoint(
    const physics::CollisionWorld& world,
    const glm::vec3& targetPoint) const {
    const glm::vec3 eyePos = controller_.interpolatedEyePosition(1.0);
    const glm::vec3 toTarget = targetPoint - eyePos;
    const float dist = glm::length(toTarget);
    if (dist <= 0.05f || dist > 45.0f) {
        return false;
    }

    const glm::vec3 dir = toTarget / dist;
    const float yawRad = glm::radians(yawDegrees_);
    const float pitchRad = glm::radians(pitchDegrees_);
    const glm::vec3 forward = glm::normalize(glm::vec3{
        std::cos(pitchRad) * std::cos(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::sin(yawRad)
    });

    const float cosHalfFov = std::cos(glm::radians(config_.fovDegrees * 0.5f));
    if (glm::dot(forward, dir) < cosHalfFov) {
        return false;
    }

    const auto hit = world.raycast(eyePos, dir, dist - 0.1f);
    return !hit.hit;
}

void BotAgent::turnTowards(const glm::vec3& targetWorldPoint, float dt) noexcept {
    const glm::vec3 eyePos = controller_.interpolatedEyePosition(1.0);
    const glm::vec3 delta = targetWorldPoint - eyePos;
    const float horizDist = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    if (horizDist < 1e-4f) {
        return;
    }

    const float desiredYaw = glm::degrees(std::atan2(delta.z, delta.x));
    const float desiredPitch = glm::degrees(std::atan2(delta.y, horizDist));

    float yawDiff = std::fmod(desiredYaw - yawDegrees_ + 540.0f, 360.0f) - 180.0f;
    const float maxStep = config_.maxTurnSpeedDegPerSec * dt;
    yawDiff = std::clamp(yawDiff, -maxStep, maxStep);
    yawDegrees_ += yawDiff;

    const float pitchDiff = std::clamp(desiredPitch - pitchDegrees_, -maxStep, maxStep);
    pitchDegrees_ = std::clamp(pitchDegrees_ + pitchDiff, -89.0f, 89.0f);
}

input::PlayerCommand BotAgent::thinkAndStep(
    const NavGraph& nav,
    const physics::CollisionWorld& world,
    const glm::vec3* enemyEyePosition,
    float dt) {
    input::PlayerCommand cmd{};
    cmd.sequence = cmdSequence_++;

    if (!isAlive() || dt <= 0.0f) {
        return cmd;
    }

    strafePhase_ += dt * 3.5f;
    const bool enemyVisible =
        (enemyEyePosition != nullptr) && canSeePoint(world, *enemyEyePosition);

    if (enemyVisible) {
        enemyReactionTimer_ += dt;
        state_ = (health_ < 30) ? BotState::SeekCover : BotState::EngageEnemy;
    } else {
        enemyReactionTimer_ = 0.0f;
        if (state_ == BotState::EngageEnemy) {
            state_ = BotState::MoveToObjective;
        }
    }

    if (state_ == BotState::EngageEnemy && enemyEyePosition != nullptr) {
        turnTowards(*enemyEyePosition, dt);
        cmd.viewYawDegrees = yawDegrees_;
        cmd.viewPitchDegrees = pitchDegrees_;
        cmd.rightMove = (std::sin(strafePhase_) >= 0.0f) ? 0.75f : -0.75f;
        cmd.forwardMove = 0.0f;

        if (enemyReactionTimer_ >= config_.reactionTimeSeconds) {
            cmd.fireHeld = true;
            cmd.fireJustPressed = true;
        }
    } else {
        // Compute or advance A* path along NavGraph
        const glm::vec3 feet = controller_.state().feetPosition;
        if (currentPath_.empty() || pathIndex_ >= currentPath_.size()) {
            const int startId = nav.findNearestWaypoint(feet);
            int goalId = -1;
            if (state_ == BotState::SeekCover) {
                goalId = nav.findWaypointByTag(WaypointTag::Cover);
            } else {
                goalId = (patrolGoalNodeId_ == nav.findWaypointByTag(WaypointTag::BombsiteA))
                    ? nav.findWaypointByTag(WaypointTag::SpawnCT)
                    : nav.findWaypointByTag(WaypointTag::BombsiteA);
            }
            if (goalId < 0 && !nav.nodes().empty()) {
                goalId = nav.nodes().back().id;
            }
            patrolGoalNodeId_ = goalId;
            currentPath_ = nav.findPathAStar(startId, goalId);
            pathIndex_ = 0;
        }

        if (pathIndex_ < currentPath_.size()) {
            const glm::vec3 wp = currentPath_[pathIndex_];
            const glm::vec3 flatDelta{wp.x - feet.x, 0.0f, wp.z - feet.z};
            if (glm::length(flatDelta) < 0.65f) {
                ++pathIndex_;
            } else {
                turnTowards(wp + glm::vec3(0.0f, 1.45f, 0.0f), dt);
                cmd.forwardMove = 1.0f;
            }
        }
        cmd.viewYawDegrees = yawDegrees_;
        cmd.viewPitchDegrees = pitchDegrees_;
    }

    controller_.simulateStep(cmd, world, dt);
    return cmd;
}

void BotAgent::applyDamage(int damage) noexcept {
    health_ = std::max(0, health_ - std::max(0, damage));
}

std::string_view BotAgent::stateName(BotState s) noexcept {
    switch (s) {
        case BotState::Patrol:
            return "PATROL";
        case BotState::MoveToObjective:
            return "MOVE_TO_OBJECTIVE";
        case BotState::EngageEnemy:
            return "ENGAGE_ENEMY";
        case BotState::SeekCover:
            return "SEEK_COVER";
    }
    return "PATROL";
}

} // namespace classicfps::ai
