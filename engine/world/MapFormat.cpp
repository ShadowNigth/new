#include "engine/world/MapFormat.hpp"
#include "engine/core/Logger.hpp"

#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace classicfps::world {

namespace {

template <typename T>
void writeBin(std::vector<std::uint8_t>& buf, const T& val) {
    const std::size_t oldSize = buf.size();
    buf.resize(oldSize + sizeof(T));
    std::memcpy(buf.data() + oldSize, &val, sizeof(T));
}

template <typename T>
bool readBin(const std::vector<std::uint8_t>& buf, std::size_t& offset, T& outVal) {
    if (offset + sizeof(T) > buf.size()) {
        return false;
    }
    std::memcpy(&outVal, buf.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

void writeStrBin(std::vector<std::uint8_t>& buf, const std::string& s) {
    const auto len = static_cast<std::uint32_t>(s.size());
    writeBin(buf, len);
    if (!s.empty()) {
        const std::size_t oldSize = buf.size();
        buf.resize(oldSize + s.size());
        std::memcpy(buf.data() + oldSize, s.data(), s.size());
    }
}

bool readStrBin(const std::vector<std::uint8_t>& buf, std::size_t& offset, std::string& outStr) {
    std::uint32_t len = 0;
    if (!readBin(buf, offset, len)) {
        return false;
    }
    if (offset + len > buf.size()) {
        return false;
    }
    outStr.assign(reinterpret_cast<const char*>(buf.data() + offset), len);
    offset += len;
    return true;
}

[[nodiscard]] EntityType parseEntityType(std::string_view str) noexcept {
    if (str == "player_spawn")  return EntityType::PlayerSpawn;
    if (str == "target_dummy")  return EntityType::TargetDummy;
    if (str == "weapon_pickup") return EntityType::WeaponPickup;
    if (str == "ammo_crate")    return EntityType::AmmoCrate;
    if (str == "point_light")   return EntityType::PointLight;
    if (str == "trigger_zone")  return EntityType::TriggerZone;
    return EntityType::TargetDummy;
}

} // namespace

std::string MapFormat::serializeToText(const MapData& map) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    out << "# ClassicFPS Engine Text Map (.cfmap) v1\n";
    out << "map_name \"" << map.name << "\"\n";
    out << "author \"" << map.author << "\"\n";
    out << "version " << map.version << "\n";
    out << "fog " << (map.fog.enabled ? 1 : 0) << " "
        << map.fog.color.r << " " << map.fog.color.g << " " << map.fog.color.b << " "
        << map.fog.startDistance << " " << map.fog.endDistance << "\n\n";

    for (const MapBrushDef& b : map.brushes) {
        out << (b.isRampNegZ ? "ramp " : "brush ")
            << b.tag << " "
            << b.center.x << " " << b.center.y << " " << b.center.z << " "
            << b.size.x << " " << b.size.y << " " << b.size.z << " "
            << b.textureId << " "
            << b.tintColor.r << " " << b.tintColor.g << " " << b.tintColor.b << " "
            << b.uvScalePerMeter << " " << (b.hasCollision ? 1 : 0) << "\n";
    }
    out << "\n";

    for (const renderer::PointLight& pl : map.pointLights) {
        out << "light "
            << pl.position.x << " " << pl.position.y << " " << pl.position.z << " "
            << pl.color.r << " " << pl.color.g << " " << pl.color.b << " "
            << pl.intensity << " " << pl.radius << "\n";
    }
    out << "\n";

    for (const MapEntityDef& ent : map.entities) {
        out << "entity "
            << entityTypeName(ent.type) << " "
            << ent.name << " "
            << ent.position.x << " " << ent.position.y << " " << ent.position.z << " "
            << ent.yawDegrees << " "
            << ent.maxHealth << " "
            << (ent.targetTag.empty() ? "-" : ent.targetTag) << " "
            << (ent.customPayload.empty() ? "-" : ent.customPayload) << "\n";
    }

    return out.str();
}

bool MapFormat::saveTextMap(const MapData& map, const std::string& filePath) {
    std::ofstream out(filePath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out << serializeToText(map);
    return out.good();
}

bool MapFormat::loadTextMapFromString(std::string_view text, MapData& outMap) {
    outMap = MapData{};
    std::istringstream stream{std::string(text)};
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ls(line);
        std::string cmd;
        if (!(ls >> cmd)) {
            continue;
        }

        if (cmd == "map_name") {
            ls >> std::quoted(outMap.name);
        } else if (cmd == "author") {
            ls >> std::quoted(outMap.author);
        } else if (cmd == "version") {
            ls >> outMap.version;
        } else if (cmd == "fog") {
            int en = 1;
            ls >> en >> outMap.fog.color.r >> outMap.fog.color.g >> outMap.fog.color.b
               >> outMap.fog.startDistance >> outMap.fog.endDistance;
            outMap.fog.enabled = (en != 0);
        } else if (cmd == "brush" || cmd == "ramp") {
            MapBrushDef b{};
            b.isRampNegZ = (cmd == "ramp");
            int col = 1;
            ls >> b.tag
               >> b.center.x >> b.center.y >> b.center.z
               >> b.size.x >> b.size.y >> b.size.z
               >> b.textureId
               >> b.tintColor.r >> b.tintColor.g >> b.tintColor.b
               >> b.uvScalePerMeter >> col;
            b.hasCollision = (col != 0);
            outMap.brushes.push_back(std::move(b));
        } else if (cmd == "light") {
            renderer::PointLight pl{};
            ls >> pl.position.x >> pl.position.y >> pl.position.z
               >> pl.color.r >> pl.color.g >> pl.color.b
               >> pl.intensity >> pl.radius;
            outMap.pointLights.push_back(pl);
        } else if (cmd == "entity") {
            std::string typeStr;
            MapEntityDef ent{};
            ls >> typeStr >> ent.name
               >> ent.position.x >> ent.position.y >> ent.position.z
               >> ent.yawDegrees >> ent.maxHealth
               >> ent.targetTag >> ent.customPayload;
            ent.type = parseEntityType(typeStr);
            if (ent.targetTag == "-") {
                ent.targetTag.clear();
            }
            if (ent.customPayload == "-") {
                ent.customPayload.clear();
            }
            outMap.entities.push_back(std::move(ent));
        }
    }

    return !outMap.brushes.empty();
}

bool MapFormat::loadTextMap(const std::string& filePath, MapData& outMap) {
    std::ifstream in(filePath, std::ios::in);
    if (!in.is_open()) {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return loadTextMapFromString(ss.str(), outMap);
}

std::vector<std::uint8_t> MapFormat::serializeToBinary(const MapData& map) {
    std::vector<std::uint8_t> buf;
    writeBin(buf, kBinaryMagic);
    writeBin(buf, kBinaryVersion);

    writeStrBin(buf, map.name);
    writeStrBin(buf, map.author);
    writeBin(buf, map.version);
    const std::uint8_t fogEn = map.fog.enabled ? 1U : 0U;
    writeBin(buf, fogEn);
    writeBin(buf, map.fog.color);
    writeBin(buf, map.fog.startDistance);
    writeBin(buf, map.fog.endDistance);

    const auto brushCount = static_cast<std::uint32_t>(map.brushes.size());
    writeBin(buf, brushCount);
    for (const MapBrushDef& b : map.brushes) {
        writeStrBin(buf, b.tag);
        const std::uint8_t rampByte = b.isRampNegZ ? 1U : 0U;
        const std::uint8_t colByte = b.hasCollision ? 1U : 0U;
        writeBin(buf, rampByte);
        writeBin(buf, b.center);
        writeBin(buf, b.size);
        writeStrBin(buf, b.textureId);
        writeBin(buf, b.tintColor);
        writeBin(buf, b.uvScalePerMeter);
        writeBin(buf, colByte);
    }

    const auto lightCount = static_cast<std::uint32_t>(map.pointLights.size());
    writeBin(buf, lightCount);
    for (const renderer::PointLight& pl : map.pointLights) {
        writeBin(buf, pl.position);
        writeBin(buf, pl.color);
        writeBin(buf, pl.intensity);
        writeBin(buf, pl.radius);
    }

    const auto entCount = static_cast<std::uint32_t>(map.entities.size());
    writeBin(buf, entCount);
    for (const MapEntityDef& ent : map.entities) {
        const auto typeByte = static_cast<std::uint8_t>(ent.type);
        writeBin(buf, typeByte);
        writeStrBin(buf, ent.name);
        writeBin(buf, ent.position);
        writeBin(buf, ent.yawDegrees);
        writeBin(buf, ent.maxHealth);
        writeStrBin(buf, ent.targetTag);
        writeStrBin(buf, ent.customPayload);
    }

    return buf;
}

bool MapFormat::saveBinaryMap(const MapData& map, const std::string& filePath) {
    const std::vector<std::uint8_t> bytes = serializeToBinary(map);
    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return out.good();
}

bool MapFormat::loadBinaryMapFromMemory(const std::vector<std::uint8_t>& bytes, MapData& outMap) {
    std::size_t offset = 0;
    std::uint32_t magic = 0;
    std::uint32_t binVer = 0;
    if (!readBin(bytes, offset, magic) || !readBin(bytes, offset, binVer)) {
        return false;
    }
    if (magic != kBinaryMagic || binVer != kBinaryVersion) {
        core::Logger::instance().error("MapFormat", "Invalid .cbsp binary map magic or version");
        return false;
    }

    outMap = MapData{};
    std::uint8_t fogEn = 1U;
    if (!readStrBin(bytes, offset, outMap.name) ||
        !readStrBin(bytes, offset, outMap.author) ||
        !readBin(bytes, offset, outMap.version) ||
        !readBin(bytes, offset, fogEn) ||
        !readBin(bytes, offset, outMap.fog.color) ||
        !readBin(bytes, offset, outMap.fog.startDistance) ||
        !readBin(bytes, offset, outMap.fog.endDistance)) {
        return false;
    }
    outMap.fog.enabled = (fogEn != 0U);

    std::uint32_t brushCount = 0;
    if (!readBin(bytes, offset, brushCount)) {
        return false;
    }
    outMap.brushes.resize(brushCount);
    for (std::uint32_t i = 0; i < brushCount; ++i) {
        MapBrushDef& b = outMap.brushes[i];
        std::uint8_t rampByte = 0U;
        std::uint8_t colByte = 1U;
        if (!readStrBin(bytes, offset, b.tag) ||
            !readBin(bytes, offset, rampByte) ||
            !readBin(bytes, offset, b.center) ||
            !readBin(bytes, offset, b.size) ||
            !readStrBin(bytes, offset, b.textureId) ||
            !readBin(bytes, offset, b.tintColor) ||
            !readBin(bytes, offset, b.uvScalePerMeter) ||
            !readBin(bytes, offset, colByte)) {
            return false;
        }
        b.isRampNegZ = (rampByte != 0U);
        b.hasCollision = (colByte != 0U);
    }

    std::uint32_t lightCount = 0;
    if (!readBin(bytes, offset, lightCount)) {
        return false;
    }
    outMap.pointLights.resize(lightCount);
    for (std::uint32_t i = 0; i < lightCount; ++i) {
        renderer::PointLight& pl = outMap.pointLights[i];
        if (!readBin(bytes, offset, pl.position) ||
            !readBin(bytes, offset, pl.color) ||
            !readBin(bytes, offset, pl.intensity) ||
            !readBin(bytes, offset, pl.radius)) {
            return false;
        }
    }

    std::uint32_t entCount = 0;
    if (!readBin(bytes, offset, entCount)) {
        return false;
    }
    outMap.entities.resize(entCount);
    for (std::uint32_t i = 0; i < entCount; ++i) {
        MapEntityDef& ent = outMap.entities[i];
        std::uint8_t typeByte = 0U;
        if (!readBin(bytes, offset, typeByte) ||
            !readStrBin(bytes, offset, ent.name) ||
            !readBin(bytes, offset, ent.position) ||
            !readBin(bytes, offset, ent.yawDegrees) ||
            !readBin(bytes, offset, ent.maxHealth) ||
            !readStrBin(bytes, offset, ent.targetTag) ||
            !readStrBin(bytes, offset, ent.customPayload)) {
            return false;
        }
        ent.type = static_cast<EntityType>(typeByte);
    }

    return !outMap.brushes.empty();
}

bool MapFormat::loadBinaryMap(const std::string& filePath, MapData& outMap) {
    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        return false;
    }
    const std::streamsize sz = in.tellg();
    if (sz <= 0) {
        return false;
    }
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(sz));
    in.read(reinterpret_cast<char*>(bytes.data()), sz);
    return loadBinaryMapFromMemory(bytes, outMap);
}

MapData MapFormat::createDefaultArenaMapData() {
    MapData map{};
    map.name = "de_arena";
    map.author = "ClassicFPS Engine";
    map.version = 1;
    map.fog.enabled = true;
    map.fog.color = {0.08f, 0.11f, 0.15f};
    map.fog.startDistance = 10.0f;
    map.fog.endDistance = 30.0f;

    map.brushes = {
        {"floor",              false, { 0.0f, -0.25f,  0.0f}, {16.0f, 0.50f, 16.0f}, "floor",    {1.0f, 1.0f, 1.0f},   0.75f, true},
        {"ceiling",            false, { 0.0f,  4.25f,  0.0f}, {16.0f, 0.50f, 16.0f}, "concrete", {0.72f, 0.76f, 0.82f}, 0.50f, true},
        {"wall_north",         false, { 0.0f,  2.00f, -8.25f}, {16.5f, 4.00f, 0.50f}, "brick",   {1.0f, 1.0f, 1.0f},   0.60f, true},
        {"wall_south",         false, { 0.0f,  2.00f,  8.25f}, {16.5f, 4.00f, 0.50f}, "brick",   {1.0f, 1.0f, 1.0f},   0.60f, true},
        {"wall_west",          false, {-8.25f, 2.00f,  0.0f}, { 0.50f, 4.00f, 16.0f}, "concrete",{1.0f, 1.0f, 1.0f},   0.60f, true},
        {"wall_east",          false, { 8.25f, 2.00f,  0.0f}, { 0.50f, 4.00f, 16.0f}, "concrete",{1.0f, 1.0f, 1.0f},   0.60f, true},
        {"pillar_nw",          false, {-3.5f,  2.00f, -3.5f}, { 1.20f, 4.00f, 1.20f}, "concrete",{0.90f, 0.92f, 0.95f}, 0.75f, true},
        {"pillar_ne",          false, { 3.5f,  2.00f, -3.5f}, { 1.20f, 4.00f, 1.20f}, "concrete",{0.90f, 0.92f, 0.95f}, 0.75f, true},
        {"pillar_sw",          false, {-3.5f,  2.00f,  3.5f}, { 1.20f, 4.00f, 1.20f}, "concrete",{0.90f, 0.92f, 0.95f}, 0.75f, true},
        {"pillar_se",          false, { 3.5f,  2.00f,  3.5f}, { 1.20f, 4.00f, 1.20f}, "concrete",{0.90f, 0.92f, 0.95f}, 0.75f, true},
        {"crate_center_a",     false, {-0.8f,  0.60f, -1.2f}, { 1.20f, 1.20f, 1.20f}, "crate",   {1.0f, 1.0f, 1.0f},   0.85f, true},
        {"crate_center_b",     false, { 0.6f,  0.60f, -1.0f}, { 1.20f, 1.20f, 1.20f}, "crate",   {0.95f, 0.90f, 0.85f}, 0.85f, true},
        {"crate_center_top",   false, {-0.1f,  1.65f, -1.1f}, { 0.90f, 0.90f, 0.90f}, "crate",   {1.0f, 0.95f, 0.88f}, 1.00f, true},
        {"step_curb_south",    false, {-2.2f,  0.175f, 1.5f}, { 1.80f, 0.35f, 1.20f}, "hazard",  {1.0f, 1.0f, 1.0f},   0.90f, true},
        {"crouch_tunnel_beam", false, { 2.2f,  1.85f,  0.8f}, { 2.40f, 1.20f, 1.60f}, "hazard",  {0.95f, 0.90f, 0.80f}, 0.80f, true},
        {"mezzanine_platform", false, { 0.0f,  0.70f, -6.2f}, { 7.50f, 1.40f, 3.20f}, "hazard",  {1.0f, 1.0f, 1.0f},   0.75f, true},
        {"mezzanine_ramp",     true,  {-2.6f,  0.70f, -3.1f}, { 2.20f, 1.40f, 3.00f}, "floor",   {0.95f, 0.92f, 0.84f}, 0.75f, true},
        {"target_torso_alpha", false, { 0.0f,  1.95f, -5.6f}, { 0.65f, 0.85f, 0.35f}, "hazard",  {1.0f, 0.85f, 0.35f},  1.20f, true},
        {"target_head_alpha",  false, { 0.0f,  2.58f, -5.6f}, { 0.36f, 0.38f, 0.35f}, "brick",   {1.0f, 0.35f, 0.30f},  1.50f, true}
    };

    map.pointLights = {
        {{-4.0f, 3.2f,  3.5f}, {1.00f, 0.86f, 0.62f}, 1.25f, 12.0f},
        {{ 4.0f, 3.2f,  1.0f}, {0.65f, 0.84f, 1.00f}, 1.15f, 12.0f},
        {{ 0.0f, 3.4f, -5.5f}, {1.00f, 0.76f, 0.45f}, 1.35f, 13.0f}
    };

    map.entities = {
        {EntityType::PlayerSpawn,  "spawn_ct_1",       {-4.4f, 0.0f,  3.0f}, -35.0f, 100.0f, "",                   ""},
        {EntityType::TargetDummy,  "dummy_alpha",      { 0.0f, 1.4f, -5.6f},   0.0f, 100.0f, "target_",            ""},
        {EntityType::WeaponPickup, "pickup_carbine",   {-2.2f, 0.5f,  1.5f},   0.0f, 100.0f, "",                   "carbine_m4"},
        {EntityType::AmmoCrate,    "ammo_supply_east", { 2.2f, 0.4f,  0.8f},   0.0f, 100.0f, "",                   "30"}
    };

    return map;
}

} // namespace classicfps::world
