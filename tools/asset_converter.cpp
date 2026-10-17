#include "engine/audio/AudioEngine.hpp"
#include "engine/resources/ModelLoader.hpp"
#include "engine/world/MapFormat.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int generateDefaultAssets() {
    using namespace classicfps;
    std::filesystem::create_directories("assets/maps");
    std::filesystem::create_directories("assets/models");
    std::filesystem::create_directories("assets/sounds");

    // 1. Maps (.cfmap and .cbsp)
    const world::MapData arena = world::MapFormat::createDefaultArenaMapData();
    if (!world::MapFormat::saveTextMap(arena, "assets/maps/de_arena.cfmap") ||
        !world::MapFormat::saveBinaryMap(arena, "assets/maps/de_arena.cbsp")) {
        std::cerr << "[ERROR] Failed to write default map assets.\n";
        return 1;
    }
    std::cout << "[OK] Generated assets/maps/de_arena.cfmap and assets/maps/de_arena.cbsp ("
              << arena.brushes.size() << " brushes, " << arena.entities.size() << " entities)\n";

    // 2. glTF 2.0 3D Models (.gltf)
    const auto carbine = resources::ModelLoader::createTacticalCarbineModel();
    const auto dummy = resources::ModelLoader::createTargetDummyModel();
    if (!resources::ModelLoader::saveGltf2ToFile(*carbine, "assets/models/tactical_carbine.gltf") ||
        !resources::ModelLoader::saveGltf2ToFile(*dummy, "assets/models/target_dummy.gltf")) {
        std::cerr << "[ERROR] Failed to write default glTF 2.0 models.\n";
        return 1;
    }
    std::cout << "[OK] Generated assets/models/tactical_carbine.gltf and assets/models/target_dummy.gltf\n";

    // 3. PCM 16-bit WAV Sounds (.wav)
    const auto fireWav = audio::SoundClip::createGunshotSound(22050);
    const auto reloadWav = audio::SoundClip::createReloadSound(22050);
    const auto dryWav = audio::SoundClip::createDryFireSound(22050);
    if (!fireWav->saveToWavFile("assets/sounds/m4_fire.wav") ||
        !reloadWav->saveToWavFile("assets/sounds/m4_reload.wav") ||
        !dryWav->saveToWavFile("assets/sounds/dryfire.wav")) {
        std::cerr << "[ERROR] Failed to write default WAV sound assets.\n";
        return 1;
    }
    std::cout << "[OK] Generated assets/sounds/m4_fire.wav, m4_reload.wav, dryfire.wav\n";
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    using namespace classicfps;

    if (argc < 2) {
        std::cout << "Usage:\n"
                  << "  classicfps_asset_converter --generate-defaults\n"
                  << "  classicfps_asset_converter --map-to-bin <input.cfmap> <output.cbsp>\n"
                  << "  classicfps_asset_converter --inspect-gltf <model.gltf>\n";
        return 0;
    }

    const std::string_view mode = argv[1];
    if (mode == "--generate-defaults") {
        return generateDefaultAssets();
    }

    if (mode == "--map-to-bin" && argc >= 4) {
        world::MapData map{};
        if (!world::MapFormat::loadTextMap(argv[2], map)) {
            std::cerr << "[ERROR] Failed to load text map: " << argv[2] << "\n";
            return 1;
        }
        if (!world::MapFormat::saveBinaryMap(map, argv[3])) {
            std::cerr << "[ERROR] Failed to save binary map: " << argv[3] << "\n";
            return 1;
        }
        std::cout << "[OK] Converted '" << argv[2] << "' -> '" << argv[3] << "' ("
                  << map.brushes.size() << " brushes, " << map.entities.size() << " entities)\n";
        return 0;
    }

    if (mode == "--inspect-gltf" && argc >= 3) {
        const auto model = resources::ModelLoader::loadGltf2FromFile(argv[2]);
        if (!model) {
            std::cerr << "[ERROR] Failed to load glTF 2.0 model: " << argv[2] << "\n";
            return 1;
        }
        std::cout << "glTF 2.0 Model: " << argv[2] << "\n"
                  << "  SubMeshes : " << model->subMeshes.size() << "\n"
                  << "  Bones     : " << model->skeleton.boneCount() << "\n"
                  << "  AnimClips : " << model->skeleton.clips().size() << "\n";
        return 0;
    }

    std::cerr << "Invalid arguments.\n";
    return 1;
}
