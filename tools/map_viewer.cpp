#include "engine/core/Engine.hpp"
#include "engine/world/MapFormat.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
    using namespace classicfps;

    std::string mapPath;
    std::string screenshotPath;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--render" && i + 1 < argc) {
            screenshotPath = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: classicfps_map_viewer [map.cfmap | map.cbsp] [--render out.ppm]\n";
            return 0;
        } else {
            mapPath = std::string(arg);
        }
    }

    world::MapData map{};
    if (!mapPath.empty() && std::filesystem::exists(mapPath)) {
        bool ok = false;
        if (mapPath.ends_with(".cbsp")) {
            ok = world::MapFormat::loadBinaryMap(mapPath, map);
        } else {
            ok = world::MapFormat::loadTextMap(mapPath, map);
        }
        if (!ok) {
            std::cerr << "[ERROR] Failed to parse map file: " << mapPath << "\n";
            return 1;
        }
    } else {
        map = world::MapFormat::createDefaultArenaMapData();
    }

    std::cout << "=== ClassicFPS Map Viewer ===\n"
              << "Map Name    : " << map.name << " (v" << map.version << ")\n"
              << "Author      : " << map.author << "\n"
              << "Brushes     : " << map.brushes.size() << "\n"
              << "PointLights : " << map.pointLights.size() << "\n"
              << "Entities    : " << map.entities.size() << "\n";

    for (const auto& ent : map.entities) {
        std::cout << "  - [" << world::entityTypeName(ent.type) << "] " << ent.name
                  << " @ (" << ent.position.x << ", " << ent.position.y << ", " << ent.position.z << ")\n";
    }

    if (!screenshotPath.empty()) {
        core::Engine engine;
        core::EngineOptions opts{};
        opts.overrideWidth = 640;
        opts.overrideHeight = 480;
        opts.maxFrames = 2;
        opts.forceHeadless = true;
        opts.screenshotPath = screenshotPath;
        if (!engine.initialize(opts)) {
            std::cerr << "[ERROR] Failed to initialize engine for map preview.\n";
            return 1;
        }
        return engine.run();
    }

    return 0;
}
