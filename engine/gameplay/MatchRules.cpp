#include "engine/gameplay/MatchRules.hpp"

#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>

namespace classicfps::gameplay {

MatchRulesEngine::MatchRulesEngine() {
    catalog_ = {
        BuyItemDef{"kevlar", "Kevlar Vest", BuyCategory::Equipment, 650, TeamId::None},
        BuyItemDef{"kevlar_helmet", "Kevlar + Helmet", BuyCategory::Equipment, 1000, TeamId::None},
        BuyItemDef{"defuse_kit", "Defuse Kit", BuyCategory::Equipment, 200, TeamId::CounterTerrorists},
        BuyItemDef{"usp", "KM .45 Tactical", BuyCategory::Pistol, 500, TeamId::None},
        BuyItemDef{"mp5", "KM Sub-Machine Gun", BuyCategory::SubmachineGun, 1500, TeamId::None},
        BuyItemDef{"m4a1", "Maverick M4A1 Carbine", BuyCategory::Rifle, 3100, TeamId::CounterTerrorists},
        BuyItemDef{"ak47", "CV-47 Assault Rifle", BuyCategory::Rifle, 2500, TeamId::Terrorists},
        BuyItemDef{"he_grenade", "HE Grenade", BuyCategory::Grenade, 300, TeamId::None},
        BuyItemDef{"flashbang", "Flashbang", BuyCategory::Grenade, 200, TeamId::None},
        BuyItemDef{"smoke_grenade", "Smoke Grenade", BuyCategory::Grenade, 300, TeamId::None}
    };
    resetMatch();
}

void MatchRulesEngine::configure(const MatchConfig& config) {
    config_ = config;
}

void MatchRulesEngine::resetMatch() {
    roundNumber_ = 0;
    ctScore_ = 0;
    terroristScore_ = 0;
    ctLossStreak_ = 0;
    terroristLossStreak_ = 0;
    lastWinReason_ = RoundWinReason::None;
    lastWinningTeam_ = TeamId::None;
    startNewRound();
}

void MatchRulesEngine::startNewRound() {
    ++roundNumber_;
    phase_ = (config_.freezeTimeSeconds > 0.0f) ? RoundPhase::FreezeTime : RoundPhase::LiveRound;
    phaseTimeRemaining_ = (phase_ == RoundPhase::FreezeTime)
        ? config_.freezeTimeSeconds
        : config_.roundTimeSeconds;
    roundElapsedSeconds_ = 0.0f;
    c4TimeRemaining_ = 0.0f;
    currentPlantProgress_ = 0.0f;
    currentDefuseProgress_ = 0.0f;
    bombExploded_ = false;
    lastWinReason_ = RoundWinReason::None;
    lastWinningTeam_ = TeamId::None;
}

void MatchRulesEngine::update(float dt) {
    if (dt <= 0.0f) {
        return;
    }

    if (phase_ == RoundPhase::FreezeTime) {
        phaseTimeRemaining_ -= dt;
        if (phaseTimeRemaining_ <= 0.0f) {
            phase_ = RoundPhase::LiveRound;
            phaseTimeRemaining_ = config_.roundTimeSeconds;
        }
        return;
    }

    if (phase_ == RoundPhase::LiveRound) {
        roundElapsedSeconds_ += dt;
        phaseTimeRemaining_ -= dt;
        if (phaseTimeRemaining_ <= 0.0f) {
            phaseTimeRemaining_ = 0.0f;
            endRound(TeamId::CounterTerrorists, RoundWinReason::RoundTimeExpired);
        }
        return;
    }

    if (phase_ == RoundPhase::BombPlanted) {
        roundElapsedSeconds_ += dt;
        c4TimeRemaining_ -= dt;
        if (c4TimeRemaining_ <= 0.0f) {
            c4TimeRemaining_ = 0.0f;
            bombExploded_ = true;
            endRound(TeamId::Terrorists, RoundWinReason::TargetBombed);
        }
        return;
    }

    if (phase_ == RoundPhase::RoundEnd) {
        phaseTimeRemaining_ -= dt;
        if (phaseTimeRemaining_ <= 0.0f) {
            startNewRound();
        }
    }
}

bool MatchRulesEngine::beginOrUpdateBombPlant(
    const glm::vec3& planterPosition,
    bool holdingUseOrPrimary,
    float dt) {
    if (phase_ != RoundPhase::LiveRound || !holdingUseOrPrimary) {
        currentPlantProgress_ = 0.0f;
        return false;
    }

    const float dist = glm::length(planterPosition - bombsiteCenter_);
    if (dist > bombsiteRadius_) {
        currentPlantProgress_ = 0.0f;
        return false;
    }

    currentPlantProgress_ += dt;
    if (currentPlantProgress_ >= config_.plantTimeSeconds) {
        currentPlantProgress_ = config_.plantTimeSeconds;
        plantedBombPos_ = planterPosition;
        phase_ = RoundPhase::BombPlanted;
        c4TimeRemaining_ = config_.c4TimerSeconds;
        phaseTimeRemaining_ = config_.c4TimerSeconds;
        return true;
    }
    return false;
}

bool MatchRulesEngine::beginOrUpdateBombDefuse(
    const glm::vec3& defuserPosition,
    bool hasDefuseKit,
    bool holdingUse,
    float dt) {
    if (phase_ != RoundPhase::BombPlanted || !holdingUse) {
        currentDefuseProgress_ = 0.0f;
        return false;
    }

    const float dist = glm::length(defuserPosition - plantedBombPos_);
    if (dist > 2.2f) {
        currentDefuseProgress_ = 0.0f;
        return false;
    }

    const float requiredTime = hasDefuseKit
        ? config_.defuseTimeWithKitSeconds
        : config_.defuseTimeNoKitSeconds;
    currentDefuseProgress_ += dt;
    if (currentDefuseProgress_ >= requiredTime) {
        currentDefuseProgress_ = requiredTime;
        endRound(TeamId::CounterTerrorists, RoundWinReason::BombDefused);
        return true;
    }
    return false;
}

void MatchRulesEngine::onTeamEliminated(TeamId eliminatedTeam) {
    if (phase_ == RoundPhase::RoundEnd) {
        return;
    }

    if (eliminatedTeam == TeamId::Terrorists) {
        // In CS 1.6, if the bomb is already planted, CTs must still defuse the bomb!
        if (phase_ != RoundPhase::BombPlanted) {
            endRound(TeamId::CounterTerrorists, RoundWinReason::TerroristsEliminated);
        }
    } else if (eliminatedTeam == TeamId::CounterTerrorists) {
        endRound(TeamId::Terrorists, RoundWinReason::CTsEliminated);
    }
}

void MatchRulesEngine::endRound(TeamId winner, RoundWinReason reason) {
    phase_ = RoundPhase::RoundEnd;
    lastWinningTeam_ = winner;
    lastWinReason_ = reason;
    phaseTimeRemaining_ = config_.roundRestartDelaySeconds;

    if (winner == TeamId::CounterTerrorists) {
        ++ctScore_;
        ctLossStreak_ = 0;
        terroristLossStreak_ = std::min(4, terroristLossStreak_ + 1);
    } else if (winner == TeamId::Terrorists) {
        ++terroristScore_;
        terroristLossStreak_ = 0;
        ctLossStreak_ = std::min(4, ctLossStreak_ + 1);
    }
}

BuyResult MatchRulesEngine::purchaseItem(
    PlayerEconomyState& player,
    std::string_view itemId) const {
    if (!player.inBuyZone) {
        return BuyResult::NotInBuyZone;
    }
    if (phase_ != RoundPhase::FreezeTime && roundElapsedSeconds_ > config_.buyTimeSeconds) {
        return BuyResult::BuyTimeExpired;
    }

    const BuyItemDef* found = nullptr;
    for (const auto& item : catalog_) {
        if (item.id == itemId) {
            found = &item;
            break;
        }
    }
    if (found == nullptr) {
        return BuyResult::UnknownItem;
    }

    if (found->restrictedTeam != TeamId::None && found->restrictedTeam != player.team) {
        return BuyResult::WrongTeam;
    }

    int effectiveCost = found->cost;
    if (found->id == "kevlar_helmet" && player.armor >= 100 && !player.hasHelmet) {
        effectiveCost = 350; // Helmet-only upgrade cost when Kevlar is full
    }

    if (found->id == "kevlar" && player.armor >= 100) {
        return BuyResult::AlreadyOwned;
    }
    if (found->id == "kevlar_helmet" && player.armor >= 100 && player.hasHelmet) {
        return BuyResult::AlreadyOwned;
    }
    if (found->id == "defuse_kit" && player.hasDefuseKit) {
        return BuyResult::AlreadyOwned;
    }
    if (found->id == "he_grenade" && player.heGrenades >= 1) {
        return BuyResult::AlreadyOwned;
    }
    if (found->id == "flashbang" && player.flashbangs >= 2) {
        return BuyResult::AlreadyOwned;
    }
    if (found->id == "smoke_grenade" && player.smokeGrenades >= 1) {
        return BuyResult::AlreadyOwned;
    }

    if (player.cash < effectiveCost) {
        return BuyResult::InsufficientFunds;
    }

    player.cash -= effectiveCost;
    if (found->id == "kevlar") {
        player.armor = 100;
    } else if (found->id == "kevlar_helmet") {
        player.armor = 100;
        player.hasHelmet = true;
    } else if (found->id == "defuse_kit") {
        player.hasDefuseKit = true;
    } else if (found->id == "he_grenade") {
        ++player.heGrenades;
    } else if (found->id == "flashbang") {
        ++player.flashbangs;
    } else if (found->id == "smoke_grenade") {
        ++player.smokeGrenades;
    } else if (found->category == BuyCategory::Pistol) {
        player.secondaryWeaponId = found->id;
    } else {
        player.primaryWeaponId = found->id;
    }

    return BuyResult::Success;
}

void MatchRulesEngine::awardKillReward(
    PlayerEconomyState& killer,
    std::string_view weaponId) const {
    int reward = 300; // Classic CS 1.6 kill reward
    if (weaponId == "mp5") {
        reward = 600;
    }
    addCash(killer, reward);
}

void MatchRulesEngine::addCash(PlayerEconomyState& player, int delta) const noexcept {
    player.cash = std::clamp(player.cash + delta, 0, config_.maxMoney);
}

float MatchRulesEngine::computeC4ExplosionDamage(const glm::vec3& victimPosition) const noexcept {
    const float dist = glm::length(victimPosition - plantedBombPos_);
    if (dist >= config_.c4DamageRadius) {
        return 0.0f;
    }
    const float falloff = 1.0f - (dist / std::max(0.01f, config_.c4DamageRadius));
    return config_.c4MaxDamage * falloff * falloff;
}

float MatchRulesEngine::plantProgressNormalized() const noexcept {
    if (config_.plantTimeSeconds <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(currentPlantProgress_ / config_.plantTimeSeconds, 0.0f, 1.0f);
}

float MatchRulesEngine::defuseProgressNormalized(bool hasDefuseKit) const noexcept {
    const float req = hasDefuseKit ? config_.defuseTimeWithKitSeconds : config_.defuseTimeNoKitSeconds;
    if (req <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(currentDefuseProgress_ / req, 0.0f, 1.0f);
}

std::string_view MatchRulesEngine::phaseName(RoundPhase phase) noexcept {
    switch (phase) {
        case RoundPhase::FreezeTime:
            return "FREEZE_TIME";
        case RoundPhase::LiveRound:
            return "LIVE_ROUND";
        case RoundPhase::BombPlanted:
            return "BOMB_PLANTED";
        case RoundPhase::RoundEnd:
            return "ROUND_END";
    }
    return "UNKNOWN";
}

} // namespace classicfps::gameplay
