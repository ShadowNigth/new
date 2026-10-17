#include "engine/core/Engine.hpp"
#include "engine/core/Logger.hpp"
#include "engine/core/Version.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void printUsage(std::string_view binaryName) {
    std::cout
        << classicfps::core::Version::buildBanner() << "\n\n"
        << "Usage: " << binaryName << " [options]\n\n"
        << "Options:\n"
        << "  --config <path>      Load engine configuration file (default: game/configs/engine.cfg)\n"
        << "  --map <path>         Load custom map file (.cbsp binary or .cfmap text)\n"
        << "  --server <port>      Start authoritative UDP server on port (e.g. 27015)\n"
        << "  --connect <ip:port>  Connect to UDP server with client prediction (e.g. 127.0.0.1:27015)\n"
        << "  --width <pixels>     Override window width\n"
        << "  --height <pixels>    Override window height\n"
        << "  --max-frames <count> Run for a fixed number of frames and exit cleanly\n"
        << "  --screenshot <path>  Save final rendered 3D frame + HUD to a PPM image file\n"
        << "  --headless           Force headless SDL2 dummy video driver\n"
        << "  --require-gl         Require strict OpenGL 3.3 Core context (disable software fallback)\n"
        << "  --version            Print engine version and exit\n"
        << "  --help               Show this help message and exit\n";
}

} // namespace

int main(int argc, char* argv[]) {
    classicfps::core::EngineOptions options{};

    const std::filesystem::path defaultCfg{"game/configs/engine.cfg"};
    if (std::filesystem::exists(defaultCfg)) {
        options.configPath = defaultCfg.string();
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (arg == "--version" || arg == "-v") {
            std::cout << classicfps::core::Version::buildBanner() << '\n';
            return EXIT_SUCCESS;
        }
        if (arg == "--headless") {
            options.forceHeadless = true;
            continue;
        }
        if (arg == "--require-gl") {
            options.requireOpenGL = true;
            continue;
        }
        if (arg == "--config" && (i + 1) < argc) {
            options.configPath = argv[++i];
            continue;
        }
        if (arg == "--map" && (i + 1) < argc) {
            options.mapPath = argv[++i];
            continue;
        }
        if (arg == "--server" && (i + 1) < argc) {
            options.serverPort = static_cast<std::uint16_t>(std::atoi(argv[++i]));
            continue;
        }
        if (arg == "--connect" && (i + 1) < argc) {
            const std::string endpoint(argv[++i]);
            const std::size_t colon = endpoint.find(':');
            if (colon != std::string::npos) {
                options.connectHost = endpoint.substr(0, colon);
                options.connectPort = static_cast<std::uint16_t>(std::atoi(endpoint.substr(colon + 1).c_str()));
            } else {
                options.connectHost = endpoint;
                options.connectPort = 27015;
            }
            continue;
        }
        if (arg == "--screenshot" && (i + 1) < argc) {
            options.screenshotPath = argv[++i];
            continue;
        }
        if (arg == "--width" && (i + 1) < argc) {
            options.overrideWidth = std::atoi(argv[++i]);
            continue;
        }
        if (arg == "--height" && (i + 1) < argc) {
            options.overrideHeight = std::atoi(argv[++i]);
            continue;
        }
        if (arg == "--max-frames" && (i + 1) < argc) {
            const long frames = std::strtol(argv[++i], nullptr, 10);
            if (frames > 0) {
                options.maxFrames = static_cast<std::uint64_t>(frames);
            }
            continue;
        }

        std::cerr << "Unknown or incomplete argument: " << arg << "\n";
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    classicfps::core::Engine engine;
    if (!engine.initialize(options)) {
        classicfps::core::Logger::instance().error(
            "Main",
            "Engine initialization failed. Exiting with error code.");
        return EXIT_FAILURE;
    }

    const int exitCode = engine.run();
    engine.shutdown();
    return exitCode;
}
