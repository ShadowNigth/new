#pragma once

#include <glm/vec3.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::gameplay {

enum class TeamId : std::uint8_t {
    None = 0,
    CounterTerrorists = 1,
    Terrorists = 2,
    Spectators = 3
};

enum class RoundPhase : std::uint8_t {
    FreezeTime = 0,
    LiveRound = 1,
    BombPlanted = 2,
    RoundEnd = 3
};

enum class RoundWinReason : std::uint8_t {
    None = 0,
    TargetBombed = 1,
    BombDefused = 2,
    TerroristsEliminated = 3,
    CTsEliminated = 4,
    RoundTimeExpired = 5
};

enum class BuyCategory : std::uint8_t {
    Pistol = 0,
    SubmachineGun = 1,
    Rifle = 2,
    Equipment = 3,
    Grenade = 4
};

struct BuyItemDef {
    std::string id;
    std::string displayName;
    BuyCategory category{BuyCategory::Rifle};
    int cost{0};
    TeamId restrictedTeam{TeamId::None}; // None = both teams can buy
};

enum class BuyResult : std::uint8_t {
    Success = 0,
    NotInBuyZone = 1,
    BuyTimeExpired = 2,
    InsufficientFunds = 3,
    WrongTeam = 4,
    AlreadyOwned = 5,
    UnknownItem = 6
};

struct PlayerEconomyState {
    TeamId team{TeamId::CounterTerrorists};
    int cash{800};
    int armor{0};
    bool hasHelmet{false};
    bool hasDefuseKit{false};
    bool hasC4{false};
    bool inBuyZone{true};
    std::string primaryWeaponId;
    std::string secondaryWeaponId{"usp"};
    int heGrenades{0};
    int flashbangs{0};
    int smokeGrenades{0};
};

struct MatchConfig {
    int startMoney{800};
    int maxMoney{16000};
    float freezeTimeSeconds{3.0f};
    float roundTimeSeconds{105.0f};
    float buyTimeSeconds{25.0f};
    float c4TimerSeconds{45.0f};
    float roundRestartDelaySeconds{4.0f};
    float defuseTimeNoKitSeconds{10.0f};
    float defuseTimeWithKitSeconds{5.0f};
    float plantTimeSeconds{3.0f};
    float c4MaxDamage{500.0f};
    float c4DamageRadius{17.5f};
};

class MatchRulesEngine {
public:
    MatchRulesEngine();

    void configure(const MatchConfig& config);
    void resetMatch();
    void startNewRound();
    void update(float dt);

    // Bomb (C4) mechanics
    [[nodiscard]] bool beginOrUpdateBombPlant(
        const glm::vec3& planterPosition,
        bool holdingUseOrPrimary,
        float dt);
    void cancelBombPlant() noexcept { currentPlantProgress_ = 0.0f; }

    [[nodiscard]] bool beginOrUpdateBombDefuse(
        const glm::vec3& defuserPosition,
        bool hasDefuseKit,
        bool holdingUse,
        float dt);
    void cancelBombDefuse() noexcept { currentDefuseProgress_ = 0.0f; }

    void onTeamEliminated(TeamId eliminatedTeam);

    // Economy & Buy Menu
    [[nodiscard]] BuyResult purchaseItem(
        PlayerEconomyState& player,
        std::string_view itemId) const;
    void awardKillReward(PlayerEconomyState& killer, std::string_view weaponId) const;
    void addCash(PlayerEconomyState& player, int delta) const noexcept;

    [[nodiscard]] float computeC4ExplosionDamage(const glm::vec3& victimPosition) const noexcept;

    void setBombsiteCenter(const glm::vec3& center, float radius = 3.5f) noexcept {
        bombsiteCenter_ = center;
        bombsiteRadius_ = radius;
    }

    // Accessors
    [[nodiscard]] RoundPhase phase() const noexcept { return phase_; }
    [[nodiscard]] RoundWinReason lastWinReason() const noexcept { return lastWinReason_; }
    [[nodiscard]] TeamId lastWinningTeam() const noexcept { return lastWinningTeam_; }
    [[nodiscard]] int roundNumber() const noexcept { return roundNumber_; }
    [[nodiscard]] int ctScore() const noexcept { return ctScore_; }
    [[nodiscard]] int terroristScore() const noexcept { return terroristScore_; }
    [[nodiscard]] float phaseTimeRemaining() const noexcept { return phaseTimeRemaining_; }
    [[nodiscard]] float c4TimeRemaining() const noexcept { return c4TimeRemaining_; }
    [[nodiscard]] float plantProgressNormalized() const noexcept;
    [[nodiscard]] float defuseProgressNormalized(bool hasDefuseKit) const noexcept;
    [[nodiscard]] bool isBombPlanted() const noexcept { return phase_ == RoundPhase::BombPlanted; }
    [[nodiscard]] bool hasBombExploded() const noexcept { return bombExploded_; }
    [[nodiscard]] const glm::vec3& plantedBombPosition() const noexcept { return plantedBombPos_; }
    [[nodiscard]] const std::vector<BuyItemDef>& buyCatalog() const noexcept { return catalog_; }
    [[nodiscard]] static std::string_view phaseName(RoundPhase phase) noexcept;

private:
    void endRound(TeamId winner, RoundWinReason reason);

    MatchConfig config_{};
    std::vector<BuyItemDef> catalog_;

    RoundPhase phase_{RoundPhase::FreezeTime};
    RoundWinReason lastWinReason_{RoundWinReason::None};
    TeamId lastWinningTeam_{TeamId::None};

    int roundNumber_{0};
    int ctScore_{0};
    int terroristScore_{0};
    int ctLossStreak_{0};
    int terroristLossStreak_{0};

    float phaseTimeRemaining_{0.0f};
    float roundElapsedSeconds_{0.0f};
    float c4TimeRemaining_{0.0f};
    float currentPlantProgress_{0.0f};
    float currentDefuseProgress_{0.0f};

    glm::vec3 bombsiteCenter_{0.0f, 0.0f, -6.0f};
    float bombsiteRadius_{3.5f};
    glm::vec3 plantedBombPos_{0.0f};
    bool bombExploded_{false};
};

} // namespace classicfps::gameplay
