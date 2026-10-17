#include "engine/ui/HudAndConsole.hpp"
#include "engine/core/Version.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace classicfps::ui {

namespace {

// Compact 5x7 bitmap font (each row is 5 bits, 7 rows per glyph)
[[nodiscard]] std::array<std::uint8_t, 7> glyph5x7(char ch) noexcept {
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    switch (c) {
        case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
        case '1': return {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
        case '2': return {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F};
        case '3': return {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E};
        case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
        case '5': return {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
        case '6': return {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
        case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
        case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
        case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};
        case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
        case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
        case 'C': return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
        case 'D': return {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C};
        case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
        case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
        case 'G': return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F};
        case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
        case 'I': return {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
        case 'J': return {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C};
        case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
        case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
        case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
        case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
        case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
        case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
        case 'Q': return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
        case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
        case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
        case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
        case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
        case 'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
        case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
        case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
        case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
        case 'Z': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
        case '+': return {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
        case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
        case '/': return {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
        case ':': return {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
        case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
        case '_': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F};
        case '|': return {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
        default:  return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    }
}

} // namespace

DiagnosticConsole::DiagnosticConsole() {
    history_.push_back("ClassicFPS Diagnostic Console Ready. Type 'help' for commands.");
}

void DiagnosticConsole::appendLine(std::string line) {
    constexpr std::size_t kMaxHistory = 64;
    if (history_.size() >= kMaxHistory) {
        history_.erase(history_.begin());
    }
    history_.push_back(std::move(line));
}

std::string DiagnosticConsole::executeCommand(
    std::string_view commandLine,
    core::Config& config,
    weapons::WeaponSystem& weaponSystem,
    world::EntityManager& entityManager) {
    appendLine("> " + std::string(commandLine));

    std::istringstream ss{std::string(commandLine)};
    std::string cmd;
    if (!(ss >> cmd)) {
        return "";
    }

    std::string response;
    if (cmd == "help") {
        response = "Commands: help, status, get <cvar>, set <cvar> <val>, give_ammo, slot <n>, reset_entities, clear";
    } else if (cmd == "status") {
        std::ostringstream oss;
        oss << core::Version::toString()
            << " | Weapon=" << (weaponSystem.activeWeapon() ? weaponSystem.activeWeapon()->def.id : "none")
            << " (" << weaponSystem.currentMagazineAmmo() << "/" << weaponSystem.currentReserveAmmo() << ")"
            << " | Entities=" << entityManager.activeCount();
        response = oss.str();
    } else if (cmd == "get") {
        std::string key;
        if (ss >> key) {
            response = key + " = \"" + config.getString(key, "<unset>") + "\"";
        } else {
            response = "Usage: get <cvar>";
        }
    } else if (cmd == "set") {
        std::string key;
        std::string val;
        if (ss >> key >> val) {
            config.setString(key, val);
            response = "Set " + key + " = " + val;
        } else {
            response = "Usage: set <cvar> <value>";
        }
    } else if (cmd == "give_ammo") {
        weaponSystem.refillAllAmmo();
        response = "Refilled all weapon magazines and reserve ammo.";
    } else if (cmd == "slot") {
        int slotIdx = 0;
        if (ss >> slotIdx) {
            if (weaponSystem.selectWeaponSlot(static_cast<std::size_t>(std::max(0, slotIdx)))) {
                response = "Switched to weapon slot " + std::to_string(slotIdx);
            } else {
                response = "Weapon slot switch ignored or out of range.";
            }
        }
    } else if (cmd == "reset_entities") {
        entityManager.update(10.0f);
        response = "Respawned destructible entities.";
    } else if (cmd == "clear") {
        history_.clear();
        return "Cleared";
    } else {
        response = "Unknown command: " + cmd + " (type 'help')";
    }

    appendLine(response);
    return response;
}

void HudRenderer::drawFilledRect(
    std::vector<std::uint32_t>& buf,
    int width,
    int height,
    int rx,
    int ry,
    int rw,
    int rh,
    std::uint32_t colorARGB) noexcept {
    if (width <= 0 || height <= 0 || buf.size() < static_cast<std::size_t>(width * height)) {
        return;
    }
    const int x0 = std::clamp(rx, 0, width);
    const int y0 = std::clamp(ry, 0, height);
    const int x1 = std::clamp(rx + rw, 0, width);
    const int y1 = std::clamp(ry + rh, 0, height);

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            buf[static_cast<std::size_t>(y * width + x)] = colorARGB;
        }
    }
}

void HudRenderer::drawText5x7(
    std::vector<std::uint32_t>& buf,
    int width,
    int height,
    int x,
    int y,
    std::string_view text,
    std::uint32_t colorARGB,
    int scale) noexcept {
    if (scale <= 0 || width <= 0 || height <= 0 ||
        buf.size() < static_cast<std::size_t>(width * height)) {
        return;
    }

    int cursorX = x;
    for (char ch : text) {
        const auto rows = glyph5x7(ch);
        for (int r = 0; r < 7; ++r) {
            const std::uint8_t rowBits = rows[static_cast<std::size_t>(r)];
            for (int c = 0; c < 5; ++c) {
                if ((rowBits & (1U << static_cast<unsigned>(4 - c))) != 0U) {
                    drawFilledRect(
                        buf,
                        width,
                        height,
                        cursorX + c * scale,
                        y + r * scale,
                        scale,
                        scale,
                        colorARGB);
                }
            }
        }
        cursorX += 6 * scale;
    }
}

void HudRenderer::renderToPixelBuffer(
    std::vector<std::uint32_t>& colorBuffer,
    int width,
    int height,
    const HudState& hud,
    const DiagnosticConsole& console) const noexcept {
    if (width < 64 || height < 64 ||
        colorBuffer.size() < static_cast<std::size_t>(width * height)) {
        return;
    }

    // Classic CS 1.6 amber & tactical green palette (ARGB8888)
    constexpr std::uint32_t kAmberColor = 0xFFFFA000U;
    constexpr std::uint32_t kCrosshairGreen = 0xFF30FF40U;
    constexpr std::uint32_t kHudBgDark = 0xFF10141AU;

    // 1. Dynamic Crosshair at screen center
    const int cx = width / 2;
    const int cy = height / 2;
    const int gap = std::clamp(static_cast<int>(3.0f + hud.currentSpreadDegrees * 3.5f), 3, 24);
    const int len = 6;

    drawFilledRect(colorBuffer, width, height, cx - gap - len, cy, len, 1, kCrosshairGreen);
    drawFilledRect(colorBuffer, width, height, cx + gap + 1,   cy, len, 1, kCrosshairGreen);
    drawFilledRect(colorBuffer, width, height, cx, cy - gap - len, 1, len, kCrosshairGreen);
    drawFilledRect(colorBuffer, width, height, cx, cy + gap + 1,   1, len, kCrosshairGreen);

    // 2. Bottom-left Health & Armor Panel
    drawFilledRect(colorBuffer, width, height, 8, height - 24, 128, 16, kHudBgDark);
    const std::string hpText =
        "+ " + std::to_string(hud.playerHealth) + "  A " + std::to_string(hud.playerArmor);
    drawText5x7(colorBuffer, width, height, 12, height - 19, hpText, kAmberColor, 1);

    // 3. Bottom-right Weapon & Ammo Panel
    drawFilledRect(colorBuffer, width, height, width - 164, height - 24, 156, 16, kHudBgDark);
    const std::string ammoText =
        hud.weaponState + " " + std::to_string(hud.magazineAmmo) + "/" + std::to_string(hud.reserveAmmo);
    drawText5x7(colorBuffer, width, height, width - 158, height - 19, ammoText, kAmberColor, 1);

    // 4. Top-right FPS & Targets Counter
    std::ostringstream topOss;
    topOss << "FPS " << static_cast<int>(std::round(hud.currentFps))
           << " HIT " << hud.targetsDestroyed;
    drawText5x7(colorBuffer, width, height, width - 120, 8, topOss.str(), kAmberColor, 1);

    // 5. Diagnostic Console Overlay (if open)
    if (console.isOpen()) {
        const int conHeight = std::max(40, (height / 2) - 24);
        drawFilledRect(colorBuffer, width, height, 0, 0, width, conHeight, 0xFF0A0E14U);
        drawFilledRect(colorBuffer, width, height, 0, conHeight - 2, width, 2, kAmberColor);

        int lineY = 8;
        const auto& hist = console.history();
        const std::size_t maxLines = static_cast<std::size_t>(std::max(1, (conHeight - 20) / 10));
        const std::size_t startIdx = (hist.size() > maxLines) ? (hist.size() - maxLines) : 0;
        for (std::size_t i = startIdx; i < hist.size(); ++i) {
            drawText5x7(colorBuffer, width, height, 8, lineY, hist[i], kAmberColor, 1);
            lineY += 10;
        }
    }
}

} // namespace classicfps::ui
