#pragma once

#include <cstdint>
#include <string>

struct SDL_Window;

namespace classicfps::platform {

struct WindowConfig {
    std::string title{"ClassicFPS Engine"};
    int width{800};
    int height{600};
    bool fullscreen{false};
    bool resizable{true};
    bool requestOpenGL{true};
    bool allowSoftwareFallback{true};
};

struct CameraInputState {
    float forwardAxis{0.0f};
    float rightAxis{0.0f};
    float upAxis{0.0f};
    float mouseDeltaX{0.0f};
    float mouseDeltaY{0.0f};
    float keyTurnYaw{0.0f};
    float keyLookPitch{0.0f};
    bool sprint{false};
    bool fireHeld{false};
    bool fireJustPressed{false};
    bool reloadPressed{false};
    int weaponSlotSelect{-1};
    bool anyManualInput{false};
};

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    [[nodiscard]] bool initialize(const WindowConfig& config);
    void shutdown() noexcept;

    void pollEvents() noexcept;
    void requestClose() noexcept { shouldClose_ = true; }

    [[nodiscard]] bool isInitialized() const noexcept { return window_ != nullptr; }
    [[nodiscard]] bool shouldClose() const noexcept { return shouldClose_; }
    [[nodiscard]] bool wasResizedThisFrame() const noexcept { return resizedThisFrame_; }
    [[nodiscard]] bool hasOpenGLFlag() const noexcept { return hasOpenGLFlag_; }

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] const std::string& videoDriverName() const noexcept { return videoDriver_; }
    [[nodiscard]] SDL_Window* nativeHandle() const noexcept { return window_; }
    [[nodiscard]] const CameraInputState& cameraInput() const noexcept { return cameraInput_; }

    void setTitle(const std::string& title);

private:
    SDL_Window* window_{nullptr};
    int width_{0};
    int height_{0};
    bool shouldClose_{false};
    bool resizedThisFrame_{false};
    bool hasOpenGLFlag_{false};
    bool sdlInitialized_{false};
    bool mouseCaptured_{false};
    std::string videoDriver_;
    CameraInputState cameraInput_{};
};

} // namespace classicfps::platform
