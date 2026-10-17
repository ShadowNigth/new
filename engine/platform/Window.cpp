#include "engine/platform/Window.hpp"
#include "engine/core/Error.hpp"
#include "engine/core/Logger.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace classicfps::platform {

Window::~Window() {
    shutdown();
}

bool Window::initialize(const WindowConfig& config) {
    if (window_ != nullptr) {
        shutdown();
    }

    width_ = std::max(64, config.width);
    height_ = std::max(64, config.height);
    shouldClose_ = false;
    resizedThisFrame_ = false;
    hasOpenGLFlag_ = false;
    mouseCaptured_ = false;
    cameraInput_ = CameraInputState{};

    SDL_version compiled{};
    SDL_version linked{};
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);

    {
        std::ostringstream oss;
        oss << "Initializing SDL2 (compiled "
            << static_cast<int>(compiled.major) << '.'
            << static_cast<int>(compiled.minor) << '.'
            << static_cast<int>(compiled.patch)
            << ", linked "
            << static_cast<int>(linked.major) << '.'
            << static_cast<int>(linked.minor) << '.'
            << static_cast<int>(linked.patch) << ")";
        core::Logger::instance().info("Window", oss.str());
    }

    const Uint32 initFlags = SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER;
    if (SDL_InitSubSystem(initFlags) != 0) {
        const std::string primaryError = SDL_GetError();
        if (!config.allowSoftwareFallback) {
            core::ErrorHandler::reportCritical(
                core::ErrorCategory::Platform,
                1001,
                "Window",
                "SDL_InitSubSystem failed: " + primaryError);
            return false;
        }

        core::Logger::instance().warn(
            "Window",
            "Native X11/Wayland video init failed (" + primaryError +
            "). Falling back to SDL 'dummy' video driver for headless Linux environment.");

        SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
        if (SDL_InitSubSystem(initFlags) != 0) {
            core::ErrorHandler::reportCritical(
                core::ErrorCategory::Platform,
                1002,
                "Window",
                std::string("SDL_InitSubSystem fallback failed: ") + SDL_GetError());
            return false;
        }
    }
    sdlInitialized_ = true;

    const char* currentDriver = SDL_GetCurrentVideoDriver();
    videoDriver_ = (currentDriver != nullptr) ? currentDriver : "unknown";
    core::Logger::instance().info("Window", "Active SDL2 video driver: " + videoDriver_);

    Uint32 baseFlags = SDL_WINDOW_SHOWN;
    if (config.resizable) {
        baseFlags |= SDL_WINDOW_RESIZABLE;
    }
    if (config.fullscreen) {
        baseFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    if (config.requestOpenGL) {
        SDL_GL_ResetAttributes();
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        window_ = SDL_CreateWindow(
            config.title.c_str(),
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            width_,
            height_,
            baseFlags | SDL_WINDOW_OPENGL);

        if (window_ != nullptr) {
            hasOpenGLFlag_ = true;
        } else {
            const std::string glWinErr = SDL_GetError();
            if (!config.allowSoftwareFallback) {
                core::ErrorHandler::reportCritical(
                    core::ErrorCategory::Platform,
                    1003,
                    "Window",
                    "SDL_CreateWindow (SDL_WINDOW_OPENGL) failed: " + glWinErr);
                shutdown();
                return false;
            }
            core::Logger::instance().warn(
                "Window",
                "Could not create SDL_WINDOW_OPENGL (" + glWinErr +
                "). Recreating SDL_Window for software framebuffer fallback.");
        }
    }

    if (window_ == nullptr) {
        window_ = SDL_CreateWindow(
            config.title.c_str(),
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            width_,
            height_,
            baseFlags);

        if (window_ == nullptr) {
            core::ErrorHandler::reportCritical(
                core::ErrorCategory::Platform,
                1004,
                "Window",
                std::string("SDL_CreateWindow failed: ") + SDL_GetError());
            shutdown();
            return false;
        }
        hasOpenGLFlag_ = false;
    }

    SDL_GetWindowSize(window_, &width_, &height_);

    std::ostringstream oss;
    oss << "Created window '" << config.title << "' (" << width_ << "x" << height_
        << ", driver=" << videoDriver_
        << ", opengl_flag=" << (hasOpenGLFlag_ ? "true" : "false") << ")";
    core::Logger::instance().info("Window", oss.str());
    return true;
}

void Window::shutdown() noexcept {
    if (window_ != nullptr) {
        core::Logger::instance().info("Window", "Destroying SDL2 window");
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (sdlInitialized_) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER);
        SDL_Quit();
        sdlInitialized_ = false;
    }
    hasOpenGLFlag_ = false;
}

void Window::pollEvents() noexcept {
    resizedThisFrame_ = false;
    cameraInput_.mouseDeltaX = 0.0f;
    cameraInput_.mouseDeltaY = 0.0f;
    cameraInput_.fireJustPressed = false;
    cameraInput_.reloadPressed = false;
    cameraInput_.weaponSlotSelect = -1;

    if (!sdlInitialized_) {
        return;
    }

    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
        switch (event.type) {
            case SDL_QUIT:
                core::Logger::instance().info("Window", "Received SDL_QUIT event");
                shouldClose_ = true;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                    core::Logger::instance().info("Window", "Received SDL_WINDOWEVENT_CLOSE");
                    shouldClose_ = true;
                } else if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                           event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    width_ = std::max(1, static_cast<int>(event.window.data1));
                    height_ = std::max(1, static_cast<int>(event.window.data2));
                    resizedThisFrame_ = true;
                }
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    core::Logger::instance().info("Window", "ESC pressed; requesting application exit");
                    shouldClose_ = true;
                } else if (event.key.keysym.sym == SDLK_TAB || event.key.keysym.sym == SDLK_m) {
                    mouseCaptured_ = !mouseCaptured_;
                    SDL_SetRelativeMouseMode(mouseCaptured_ ? SDL_TRUE : SDL_FALSE);
                } else if (event.key.keysym.sym == SDLK_r) {
                    cameraInput_.reloadPressed = true;
                    cameraInput_.anyManualInput = true;
                } else if (event.key.keysym.sym == SDLK_f && event.key.repeat == 0) {
                    cameraInput_.fireJustPressed = true;
                    cameraInput_.anyManualInput = true;
                } else if (event.key.keysym.sym == SDLK_1) {
                    cameraInput_.weaponSlotSelect = 0;
                    cameraInput_.anyManualInput = true;
                } else if (event.key.keysym.sym == SDLK_2) {
                    cameraInput_.weaponSlotSelect = 1;
                    cameraInput_.anyManualInput = true;
                } else if (event.key.keysym.sym == SDLK_3) {
                    cameraInput_.weaponSlotSelect = 2;
                    cameraInput_.anyManualInput = true;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    cameraInput_.fireJustPressed = true;
                    cameraInput_.anyManualInput = true;
                }
                break;

            case SDL_MOUSEMOTION:
                if (mouseCaptured_ || (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0U) {
                    cameraInput_.mouseDeltaX += static_cast<float>(event.motion.xrel);
                    cameraInput_.mouseDeltaY += static_cast<float>(event.motion.yrel);
                    cameraInput_.anyManualInput = true;
                }
                break;

            default:
                break;
        }
    }

    const Uint32 mouseButtons = SDL_GetMouseState(nullptr, nullptr);
    int numKeys = 0;
    const Uint8* keys = SDL_GetKeyboardState(&numKeys);
    if (keys != nullptr) {
        float fwd = 0.0f;
        if (keys[SDL_SCANCODE_W]) fwd += 1.0f;
        if (keys[SDL_SCANCODE_S]) fwd -= 1.0f;

        float right = 0.0f;
        if (keys[SDL_SCANCODE_D]) right += 1.0f;
        if (keys[SDL_SCANCODE_A]) right -= 1.0f;

        float up = 0.0f;
        if (keys[SDL_SCANCODE_SPACE]) up += 1.0f;
        if (keys[SDL_SCANCODE_C] || keys[SDL_SCANCODE_LCTRL]) up -= 1.0f;

        float yawTurn = 0.0f;
        if (keys[SDL_SCANCODE_RIGHT]) yawTurn += 1.0f;
        if (keys[SDL_SCANCODE_LEFT])  yawTurn -= 1.0f;

        float pitchLook = 0.0f;
        if (keys[SDL_SCANCODE_UP])   pitchLook += 1.0f;
        if (keys[SDL_SCANCODE_DOWN]) pitchLook -= 1.0f;

        cameraInput_.forwardAxis = fwd;
        cameraInput_.rightAxis = right;
        cameraInput_.upAxis = up;
        cameraInput_.keyTurnYaw = yawTurn;
        cameraInput_.keyLookPitch = pitchLook;
        cameraInput_.sprint = (keys[SDL_SCANCODE_LSHIFT] != 0 || keys[SDL_SCANCODE_RSHIFT] != 0);
        cameraInput_.fireHeld = ((mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0U) ||
                                (keys[SDL_SCANCODE_F] != 0);

        if (std::fabs(fwd) > 0.01f || std::fabs(right) > 0.01f || std::fabs(up) > 0.01f ||
            std::fabs(yawTurn) > 0.01f || std::fabs(pitchLook) > 0.01f || cameraInput_.fireHeld) {
            cameraInput_.anyManualInput = true;
        }
    }
}

void Window::setTitle(const std::string& title) {
    if (window_ != nullptr) {
        SDL_SetWindowTitle(window_, title.c_str());
    }
}

} // namespace classicfps::platform
