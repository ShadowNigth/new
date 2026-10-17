#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::weapons {

enum class FireType : std::uint8_t {
    HitscanRaycast = 0,
    PhysicalProjectile = 1
};

enum class FireMode : std::uint8_t {
    SemiAutomatic = 0,
    Automatic = 1,
    Burst3 = 2
};

struct WeaponDefinition {
    std::string id{"carbine_m4"};
    std::string displayName{"M4A1 Tactical Carbine"};

    FireType fireType{FireType::HitscanRaycast};
    FireMode fireMode{FireMode::Automatic};

    float damage{32.0f};
    float roundsPerMinute{650.0f};
    int magazineCapacity{30};
    int initialReserveAmmo{90};
    int maxReserveAmmo{120};

    float reloadTimeSeconds{2.1f};
    float switchTimeSeconds{0.65f};
    float maxRangeMeters{80.0f};

    // Projectile parameters (used when fireType == PhysicalProjectile)
    float projectileSpeed{28.0f};
    float projectileGravity{9.8f};

    // Accuracy / spread parameters
    float baseSpreadDegrees{0.45f};
    float moveSpreadMultiplier{2.6f};
    float crouchSpreadMultiplier{0.60f};
    float airSpreadMultiplier{4.2f};

    // Recoil parameters
    float recoilPitchDegrees{1.35f};
    float recoilYawDegrees{0.45f};
    float recoilRecoverySpeed{9.0f};

    // Sound & animation identifiers
    std::string soundFire{"weapons/m4_fire.wav"};
    std::string soundReload{"weapons/m4_reload.wav"};
    std::string soundEmpty{"weapons/dryfire.wav"};
    std::string animIdle{"idle"};
    std::string animFire{"fire"};
    std::string animReload{"reload"};
    std::string animDraw{"draw"};

    [[nodiscard]] float fireIntervalSeconds() const noexcept {
        return (roundsPerMinute > 1.0f) ? (60.0f / roundsPerMinute) : 0.5f;
    }
};

class WeaponLibrary {
public:
    WeaponLibrary();

    void loadDefaults();
    [[nodiscard]] bool loadFromFile(const std::string& filePath);
    [[nodiscard]] bool loadFromString(std::string_view content);

    void registerWeapon(WeaponDefinition def);
    [[nodiscard]] const WeaponDefinition* findById(std::string_view id) const noexcept;
    [[nodiscard]] const WeaponDefinition* findByIndex(std::size_t index) const noexcept;
    [[nodiscard]] std::size_t count() const noexcept { return definitions_.size(); }
    [[nodiscard]] const std::vector<WeaponDefinition>& definitions() const noexcept {
        return definitions_;
    }

private:
    std::vector<WeaponDefinition> definitions_;
};

} // namespace classicfps::weapons
