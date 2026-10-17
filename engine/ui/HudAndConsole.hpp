#pragma once

#include "engine/core/Config.hpp"
#include "engine/debug/FrameStats.hpp"
#include "engine/weapons/WeaponSystem.hpp"
#include "engine/world/EntityManager.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::ui {

class DiagnosticConsole {
public:
    DiagnosticConsole();

    void toggle() noexcept { open_ = !open_; }
    void setOpen(bool open) noexcept { open_ = open; }
    [[nodiscard]] bool isOpen() const noexcept { return open_; }

    std::string executeCommand(
        std::string_view commandLine,
        core::Config& config,
        weapons::WeaponSystem& weaponSystem,
        world::EntityManager& entityManager);

    void appendLine(std::string line);
    void clearHistory() noexcept { history_.clear(); }

    [[nodiscard]] const std::vector<std::string>& history() const noexcept { return history_; }

private:
    bool open_{false};
    std::vector<std::string> history_;
};

struct HudState {
    int playerHealth{100};
    int playerArmor{100};
    float currentSpreadDegrees{0.45f};
    std::string weaponName{"M4A1"};
    std::string weaponState{"Idle"};
    int magazineAmmo{30};
    int reserveAmmo{90};
    double currentFps{100.0};
    double avgFrameMs{10.0};
    std::uint32_t targetsDestroyed{0};
    bool hitMarkerActive{false};
};

class HudRenderer {
public:
    HudRenderer() = default;

    // Draws the classic CS 1.6 amber HUD, dynamic spread crosshair, and optional console
    // directly into an ARGB8888 pixel buffer of size width x height.
    void renderToPixelBuffer(
        std::vector<std::uint32_t>& colorBuffer,
        int width,
        int height,
        const HudState& hud,
        const DiagnosticConsole& console) const noexcept;

    // Draws a 5x7 uppercase/ASCII string into an ARGB8888 buffer
    static void drawText5x7(
        std::vector<std::uint32_t>& buf,
        int width,
        int height,
        int x,
        int y,
        std::string_view text,
        std::uint32_t colorARGB,
        int scale = 1) noexcept;

    static void drawFilledRect(
        std::vector<std::uint32_t>& buf,
        int width,
        int height,
        int rx,
        int ry,
        int rw,
        int rh,
        std::uint32_t colorARGB) noexcept;
};

} // namespace classicfps::ui
