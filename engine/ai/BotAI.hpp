#pragma once

#include "engine/input/InputManager.hpp"
#include "engine/physics/CollisionWorld.hpp"
#include "engine/physics/FPSController.hpp"

#include <glm/vec3.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::ai {

enum class WaypointTag : std::uint8_t {
    General = 0,
    SpawnCT = 1,
    SpawnT = 2,
    BombsiteA = 3,
    Cover = 4,
    MidPatrol = 5
};

struct WaypointNode {
    int id{0};
    glm::vec3 position{0.0f};
    WaypointTag tag{WaypointTag::General};
    std::vector<int> neighbors;
};

class NavGraph {
public:
    void clear();
    int addWaypoint(const glm::vec3& position, WaypointTag tag = WaypointTag::General);
    void connectWaypoints(int idA, int idB, bool bidirectional = true);
    void autoLinkByVisibility(
        const physics::CollisionWorld& world,
        float maxLinkDistance = 12.0f);

    void buildDefaultArenaGraph(const physics::CollisionWorld& world);

    [[nodiscard]] int findNearestWaypoint(const glm::vec3& pos) const noexcept;
    [[nodiscard]] int findWaypointByTag(WaypointTag tag) const noexcept;
    [[nodiscard]] std::vector<glm::vec3> findPathAStar(int startNodeId, int goalNodeId) const;
    [[nodiscard]] const std::vector<WaypointNode>& nodes() const noexcept { return nodes_; }

private:
    [[nodiscard]] const WaypointNode* getNode(int id) const noexcept;
    std::vector<WaypointNode> nodes_;
};

enum class BotState : std::uint8_t {
    Patrol = 0,
    MoveToObjective = 1,
    EngageEnemy = 2,
    SeekCover = 3
};

struct BotConfig {
    std::string name{"Bot_Alpha"};
    float skill{0.80f}; // 0.0 (rookie) .. 1.0 (expert)
    float reactionTimeSeconds{0.18f};
    float maxTurnSpeedDegPerSec{360.0f};
    float fovDegrees{105.0f};
};

class BotAgent {
public:
    BotAgent() = default;
    explicit BotAgent(const BotConfig& cfg);

    void spawn(const glm::vec3& feetPosition, float yawDegrees);

    [[nodiscard]] input::PlayerCommand thinkAndStep(
        const NavGraph& nav,
        const physics::CollisionWorld& world,
        const glm::vec3* enemyEyePosition,
        float dt);

    [[nodiscard]] bool canSeePoint(
        const physics::CollisionWorld& world,
        const glm::vec3& targetPoint) const;

    void applyDamage(int damage) noexcept;

    [[nodiscard]] BotState state() const noexcept { return state_; }
    [[nodiscard]] static std::string_view stateName(BotState s) noexcept;
    [[nodiscard]] int health() const noexcept { return health_; }
    [[nodiscard]] bool isAlive() const noexcept { return health_ > 0; }
    [[nodiscard]] const physics::FPSController& controller() const noexcept { return controller_; }
    [[nodiscard]] physics::FPSController& controller() noexcept { return controller_; }
    [[nodiscard]] const std::vector<glm::vec3>& currentPath() const noexcept { return currentPath_; }
    [[nodiscard]] const BotConfig& config() const noexcept { return config_; }

private:
    void turnTowards(const glm::vec3& targetWorldPoint, float dt) noexcept;

    BotConfig config_{};
    physics::FPSController controller_{};
    BotState state_{BotState::Patrol};
    int health_{100};
    float yawDegrees_{0.0f};
    float pitchDegrees_{0.0f};
    float enemyReactionTimer_{0.0f};
    float strafePhase_{0.0f};
    std::uint32_t cmdSequence_{1};

    std::vector<glm::vec3> currentPath_;
    std::size_t pathIndex_{0};
    int patrolGoalNodeId_{-1};
};

} // namespace classicfps::ai
