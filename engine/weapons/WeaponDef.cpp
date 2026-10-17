#include "engine/weapons/WeaponDef.hpp"
#include "engine/core/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

namespace classicfps::weapons {

namespace {

[[nodiscard]] std::string trim(std::string_view input) {
    std::size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
        ++start;
    }
    std::size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }
    return std::string(input.substr(start, end - start));
}

[[nodiscard]] std::string stripQuotes(std::string_view input) {
    if (input.size() >= 2 &&
        ((input.front() == '"' && input.back() == '"') ||
         (input.front() == '\'' && input.back() == '\''))) {
        return std::string(input.substr(1, input.size() - 2));
    }
    return std::string(input);
}

void applyProperty(WeaponDefinition& def, const std::string& key, const std::string& val) {
    if (key == "display_name") {
        def.displayName = val;
    } else if (key == "fire_type") {
        def.fireType = (val == "projectile")
            ? FireType::PhysicalProjectile
            : FireType::HitscanRaycast;
    } else if (key == "fire_mode") {
        if (val == "auto") def.fireMode = FireMode::Automatic;
        else if (val == "burst") def.fireMode = FireMode::Burst3;
        else def.fireMode = FireMode::SemiAutomatic;
    } else if (key == "damage") {
        def.damage = std::max(0.0f, std::strtof(val.c_str(), nullptr));
    } else if (key == "rpm") {
        def.roundsPerMinute = std::max(10.0f, std::strtof(val.c_str(), nullptr));
    } else if (key == "magazine_capacity") {
        def.magazineCapacity = std::max(1, std::atoi(val.c_str()));
    } else if (key == "reserve_ammo") {
        def.initialReserveAmmo = std::max(0, std::atoi(val.c_str()));
    } else if (key == "max_reserve_ammo") {
        def.maxReserveAmmo = std::max(0, std::atoi(val.c_str()));
    } else if (key == "reload_time") {
        def.reloadTimeSeconds = std::max(0.1f, std::strtof(val.c_str(), nullptr));
    } else if (key == "switch_time") {
        def.switchTimeSeconds = std::max(0.05f, std::strtof(val.c_str(), nullptr));
    } else if (key == "max_range") {
        def.maxRangeMeters = std::max(1.0f, std::strtof(val.c_str(), nullptr));
    } else if (key == "projectile_speed") {
        def.projectileSpeed = std::max(1.0f, std::strtof(val.c_str(), nullptr));
    } else if (key == "projectile_gravity") {
        def.projectileGravity = std::max(0.0f, std::strtof(val.c_str(), nullptr));
    } else if (key == "base_spread_deg") {
        def.baseSpreadDegrees = std::max(0.0f, std::strtof(val.c_str(), nullptr));
    } else if (key == "move_spread_mult") {
        def.moveSpreadMultiplier = std::max(0.1f, std::strtof(val.c_str(), nullptr));
    } else if (key == "crouch_spread_mult") {
        def.crouchSpreadMultiplier = std::max(0.1f, std::strtof(val.c_str(), nullptr));
    } else if (key == "air_spread_mult") {
        def.airSpreadMultiplier = std::max(0.1f, std::strtof(val.c_str(), nullptr));
    } else if (key == "recoil_pitch_deg") {
        def.recoilPitchDegrees = std::max(0.0f, std::strtof(val.c_str(), nullptr));
    } else if (key == "recoil_yaw_deg") {
        def.recoilYawDegrees = std::max(0.0f, std::strtof(val.c_str(), nullptr));
    } else if (key == "recoil_recovery_speed") {
        def.recoilRecoverySpeed = std::max(0.1f, std::strtof(val.c_str(), nullptr));
    } else if (key == "sound_fire") {
        def.soundFire = val;
    } else if (key == "sound_reload") {
        def.soundReload = val;
    } else if (key == "sound_empty") {
        def.soundEmpty = val;
    }
}

} // namespace

WeaponLibrary::WeaponLibrary() {
    loadDefaults();
}

void WeaponLibrary::loadDefaults() {
    definitions_.clear();

    WeaponDefinition m4{};
    m4.id = "carbine_m4";
    m4.displayName = "M4A1 Tactical Carbine";
    m4.fireType = FireType::HitscanRaycast;
    m4.fireMode = FireMode::Automatic;
    m4.damage = 32.0f;
    m4.roundsPerMinute = 650.0f;
    m4.magazineCapacity = 30;
    m4.initialReserveAmmo = 90;
    m4.maxReserveAmmo = 120;
    m4.reloadTimeSeconds = 2.1f;
    m4.switchTimeSeconds = 0.65f;
    m4.maxRangeMeters = 80.0f;
    registerWeapon(std::move(m4));

    WeaponDefinition usp{};
    usp.id = "tactical_usp";
    usp.displayName = "USP .45 Tactical Pistol";
    usp.fireType = FireType::HitscanRaycast;
    usp.fireMode = FireMode::SemiAutomatic;
    usp.damage = 34.0f;
    usp.roundsPerMinute = 400.0f;
    usp.magazineCapacity = 12;
    usp.initialReserveAmmo = 48;
    usp.maxReserveAmmo = 100;
    usp.reloadTimeSeconds = 1.6f;
    usp.switchTimeSeconds = 0.45f;
    usp.maxRangeMeters = 60.0f;
    usp.baseSpreadDegrees = 0.30f;
    usp.recoilPitchDegrees = 1.80f;
    registerWeapon(std::move(usp));

    WeaponDefinition gl{};
    gl.id = "launcher_gl";
    gl.displayName = "XM-40 Impulse Launcher";
    gl.fireType = FireType::PhysicalProjectile;
    gl.fireMode = FireMode::SemiAutomatic;
    gl.damage = 85.0f;
    gl.roundsPerMinute = 90.0f;
    gl.magazineCapacity = 6;
    gl.initialReserveAmmo = 18;
    gl.maxReserveAmmo = 24;
    gl.reloadTimeSeconds = 2.6f;
    gl.switchTimeSeconds = 0.80f;
    gl.maxRangeMeters = 65.0f;
    gl.projectileSpeed = 28.0f;
    gl.projectileGravity = 9.8f;
    registerWeapon(std::move(gl));
}

bool WeaponLibrary::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        core::Logger::instance().warn("WeaponLibrary", "Could not open weapons file: " + filePath);
        return false;
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    const bool ok = loadFromString(oss.str());
    if (ok) {
        core::Logger::instance().info(
            "WeaponLibrary",
            "Loaded " + std::to_string(definitions_.size()) + " weapon definitions from " + filePath);
    }
    return ok;
}

bool WeaponLibrary::loadFromString(std::string_view content) {
    std::istringstream stream{std::string(content)};
    std::string rawLine;
    WeaponDefinition current{};
    bool hasCurrent = false;

    while (std::getline(stream, rawLine)) {
        const std::size_t commentPos = rawLine.find('#');
        const std::string line = trim(
            (commentPos == std::string::npos)
                ? std::string_view(rawLine)
                : std::string_view(rawLine).substr(0, commentPos));
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[' && line.back() == ']' && line.size() > 2) {
            if (hasCurrent && !current.id.empty()) {
                registerWeapon(current);
            }
            current = WeaponDefinition{};
            current.id = trim(std::string_view(line).substr(1, line.size() - 2));
            hasCurrent = true;
            continue;
        }

        const std::size_t eqPos = line.find('=');
        if (eqPos != std::string::npos && hasCurrent) {
            const std::string key = trim(std::string_view(line).substr(0, eqPos));
            const std::string val = stripQuotes(trim(std::string_view(line).substr(eqPos + 1)));
            applyProperty(current, key, val);
        }
    }

    if (hasCurrent && !current.id.empty()) {
        registerWeapon(current);
    }
    return !definitions_.empty();
}

void WeaponLibrary::registerWeapon(WeaponDefinition def) {
    for (WeaponDefinition& existing : definitions_) {
        if (existing.id == def.id) {
            existing = std::move(def);
            return;
        }
    }
    definitions_.push_back(std::move(def));
}

const WeaponDefinition* WeaponLibrary::findById(std::string_view id) const noexcept {
    for (const WeaponDefinition& def : definitions_) {
        if (def.id == id) {
            return &def;
        }
    }
    return nullptr;
}

const WeaponDefinition* WeaponLibrary::findByIndex(std::size_t index) const noexcept {
    if (index >= definitions_.size()) {
        return nullptr;
    }
    return &definitions_[index];
}

} // namespace classicfps::weapons
