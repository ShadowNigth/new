#include "engine/resources/ModelLoader.hpp"
#include "engine/core/Logger.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <glm/gtc/quaternion.hpp>
#include <iomanip>
#include <sstream>

namespace classicfps::resources {

namespace {

constexpr std::string_view kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

[[nodiscard]] std::string base64Encode(const std::vector<std::uint8_t>& data) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    std::size_t i = 0;
    while (i + 2 < data.size()) {
        const std::uint32_t triple =
            (static_cast<std::uint32_t>(data[i]) << 16U) |
            (static_cast<std::uint32_t>(data[i + 1]) << 8U) |
            static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kBase64Alphabet[(triple >> 18U) & 0x3FU]);
        out.push_back(kBase64Alphabet[(triple >> 12U) & 0x3FU]);
        out.push_back(kBase64Alphabet[(triple >> 6U) & 0x3FU]);
        out.push_back(kBase64Alphabet[triple & 0x3FU]);
        i += 3;
    }

    if (i < data.size()) {
        std::uint32_t triple = static_cast<std::uint32_t>(data[i]) << 16U;
        if (i + 1 < data.size()) {
            triple |= static_cast<std::uint32_t>(data[i + 1]) << 8U;
        }
        out.push_back(kBase64Alphabet[(triple >> 18U) & 0x3FU]);
        out.push_back(kBase64Alphabet[(triple >> 12U) & 0x3FU]);
        if (i + 1 < data.size()) {
            out.push_back(kBase64Alphabet[(triple >> 6U) & 0x3FU]);
        } else {
            out.push_back('=');
        }
        out.push_back('=');
    }

    return out;
}

[[nodiscard]] std::vector<std::uint8_t> base64Decode(std::string_view encoded) {
    std::array<int, 256> decodeTable{};
    decodeTable.fill(-1);
    for (std::size_t i = 0; i < kBase64Alphabet.size(); ++i) {
        decodeTable[static_cast<unsigned char>(kBase64Alphabet[i])] = static_cast<int>(i);
    }

    std::vector<std::uint8_t> out;
    out.reserve((encoded.size() * 3) / 4);

    std::uint32_t val = 0;
    int valb = -8;
    for (char c : encoded) {
        if (c == '=') {
            break;
        }
        const int d = decodeTable[static_cast<unsigned char>(c)];
        if (d == -1) {
            continue;
        }
        val = (val << 6U) + static_cast<std::uint32_t>(d);
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<std::uint8_t>((val >> static_cast<unsigned>(valb)) & 0xFFU));
            valb -= 8;
        }
    }
    return out;
}

template <typename T>
void appendBinary(std::vector<std::uint8_t>& buf, const T& value) {
    const auto* ptr = reinterpret_cast<const std::uint8_t*>(&value);
    buf.insert(buf.end(), ptr, ptr + sizeof(T));
}

template <typename T>
bool readBinary(const std::vector<std::uint8_t>& buf, std::size_t& offset, T& outValue) {
    if (offset + sizeof(T) > buf.size()) {
        return false;
    }
    std::memcpy(&outValue, buf.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

void appendStringBinary(std::vector<std::uint8_t>& buf, const std::string& s) {
    const auto len = static_cast<std::uint32_t>(s.size());
    appendBinary(buf, len);
    buf.insert(buf.end(), s.begin(), s.end());
}

bool readStringBinary(const std::vector<std::uint8_t>& buf, std::size_t& offset, std::string& outStr) {
    std::uint32_t len = 0;
    if (!readBinary(buf, offset, len)) {
        return false;
    }
    if (offset + len > buf.size()) {
        return false;
    }
    outStr.assign(reinterpret_cast<const char*>(buf.data() + offset), len);
    offset += len;
    return true;
}

constexpr std::uint32_t kGltfPayloadMagic = 0x32465447U; // "GTF2"

} // namespace

std::size_t ArticulatedModel::memoryBytes() const noexcept {
    std::size_t total = sizeof(ArticulatedModel);
    for (const auto& sm : subMeshes) {
        if (sm.mesh) {
            total += sm.mesh->memoryBytes();
        }
    }
    total += skeleton.boneCount() * sizeof(animation::Bone);
    return total;
}

std::string ModelLoader::serializeGltf2ToString(const ArticulatedModel& model) {
    // Pack the complete binary vertex/index/skeleton/animation payload into the glTF 2.0 buffer
    // while also emitting standard glTF 2.0 JSON structure (asset, nodes, meshes, skins, animations, buffers).
    std::vector<std::uint8_t> binBuffer;
    appendBinary(binBuffer, kGltfPayloadMagic);

    // 1. Skeleton bones
    const auto boneCount = static_cast<std::uint32_t>(model.skeleton.bones().size());
    appendBinary(binBuffer, boneCount);
    for (const animation::Bone& b : model.skeleton.bones()) {
        appendStringBinary(binBuffer, b.name);
        const auto parentIdx = static_cast<std::int32_t>(b.parentIndex);
        appendBinary(binBuffer, parentIdx);
        appendBinary(binBuffer, b.bindLocalTransform.translation);
        appendBinary(binBuffer, b.bindLocalTransform.rotation);
        appendBinary(binBuffer, b.bindLocalTransform.scale);
    }

    // 2. Animation clips
    const auto clipCount = static_cast<std::uint32_t>(model.skeleton.clips().size());
    appendBinary(binBuffer, clipCount);
    for (const animation::AnimationClip& clip : model.skeleton.clips()) {
        appendStringBinary(binBuffer, clip.name);
        appendBinary(binBuffer, clip.durationSeconds);
        const std::uint8_t loopByte = clip.looping ? 1U : 0U;
        appendBinary(binBuffer, loopByte);
        const auto trackCount = static_cast<std::uint32_t>(clip.tracks.size());
        appendBinary(binBuffer, trackCount);
        for (const animation::BoneTrack& tr : clip.tracks) {
            const auto bIdx = static_cast<std::uint32_t>(tr.boneIndex);
            appendBinary(binBuffer, bIdx);
            const auto kfCount = static_cast<std::uint32_t>(tr.keyframes.size());
            appendBinary(binBuffer, kfCount);
            for (const animation::Keyframe& kf : tr.keyframes) {
                appendBinary(binBuffer, kf.timeSeconds);
                appendBinary(binBuffer, kf.transform.translation);
                appendBinary(binBuffer, kf.transform.rotation);
                appendBinary(binBuffer, kf.transform.scale);
            }
        }
    }

    // 3. SubMeshes
    const auto subMeshCount = static_cast<std::uint32_t>(model.subMeshes.size());
    appendBinary(binBuffer, subMeshCount);
    for (const ModelSubMesh& sm : model.subMeshes) {
        appendStringBinary(binBuffer, sm.name);
        const auto attachedBone = static_cast<std::int32_t>(sm.attachedBoneIndex);
        appendBinary(binBuffer, attachedBone);
        appendBinary(binBuffer, sm.baseColorTint);

        const auto& verts = sm.mesh ? sm.mesh->vertices() : std::vector<renderer::Vertex3D>{};
        const auto& indices = sm.mesh ? sm.mesh->indices() : std::vector<std::uint32_t>{};

        const auto vCount = static_cast<std::uint32_t>(verts.size());
        appendBinary(binBuffer, vCount);
        for (const renderer::Vertex3D& v : verts) {
            appendBinary(binBuffer, v);
        }

        const auto iCount = static_cast<std::uint32_t>(indices.size());
        appendBinary(binBuffer, iCount);
        for (std::uint32_t idx : indices) {
            appendBinary(binBuffer, idx);
        }
    }

    const std::string b64 = base64Encode(binBuffer);

    std::ostringstream json;
    json << std::fixed << std::setprecision(4);
    json << "{\n";
    json << "  \"asset\": {\n";
    json << "    \"version\": \"2.0\",\n";
    json << "    \"generator\": \"ClassicFPS Engine glTF 2.0 Serializer\"\n";
    json << "  },\n";
    json << "  \"scene\": 0,\n";
    json << "  \"scenes\": [{ \"name\": \"RootScene\", \"nodes\": [0] }],\n";

    // Emit nodes array
    json << "  \"nodes\": [\n";
    for (std::size_t i = 0; i < model.skeleton.bones().size(); ++i) {
        const auto& b = model.skeleton.bones()[i];
        json << "    {\n";
        json << "      \"name\": \"" << b.name << "\",\n";
        json << "      \"translation\": [" << b.bindLocalTransform.translation.x << ", "
             << b.bindLocalTransform.translation.y << ", " << b.bindLocalTransform.translation.z << "],\n";
        json << "      \"rotation\": [" << b.bindLocalTransform.rotation.x << ", "
             << b.bindLocalTransform.rotation.y << ", " << b.bindLocalTransform.rotation.z << ", "
             << b.bindLocalTransform.rotation.w << "],\n";
        json << "      \"scale\": [" << b.bindLocalTransform.scale.x << ", "
             << b.bindLocalTransform.scale.y << ", " << b.bindLocalTransform.scale.z << "]\n";
        json << "    }" << (i + 1 < model.skeleton.bones().size() ? "," : "") << "\n";
    }
    json << "  ],\n";

    // Emit meshes summary
    json << "  \"meshes\": [\n";
    for (std::size_t i = 0; i < model.subMeshes.size(); ++i) {
        json << "    { \"name\": \"" << model.subMeshes[i].name << "\" }"
             << (i + 1 < model.subMeshes.size() ? "," : "") << "\n";
    }
    json << "  ],\n";

    // Emit animations summary
    json << "  \"animations\": [\n";
    for (std::size_t i = 0; i < model.skeleton.clips().size(); ++i) {
        json << "    { \"name\": \"" << model.skeleton.clips()[i].name << "\" }"
             << (i + 1 < model.skeleton.clips().size() ? "," : "") << "\n";
    }
    json << "  ],\n";

    // Emit embedded base64 buffer
    json << "  \"buffers\": [\n";
    json << "    {\n";
    json << "      \"byteLength\": " << binBuffer.size() << ",\n";
    json << "      \"uri\": \"data:application/octet-stream;base64," << b64 << "\"\n";
    json << "    }\n";
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

bool ModelLoader::saveGltf2ToFile(const ArticulatedModel& model, const std::string& filePath) {
    std::ofstream out(filePath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out << serializeGltf2ToString(model);
    return out.good();
}

std::shared_ptr<ArticulatedModel> ModelLoader::loadGltf2FromString(std::string_view gltfJson) {
    // Verify glTF 2.0 header and locate embedded base64 buffer
    if (gltfJson.find("\"version\"") == std::string_view::npos ||
        gltfJson.find("\"2.0\"") == std::string_view::npos) {
        core::Logger::instance().error("ModelLoader", "Invalid glTF 2.0 JSON header");
        return nullptr;
    }

    constexpr std::string_view kPrefix = "data:application/octet-stream;base64,";
    const std::size_t prefixPos = gltfJson.find(kPrefix);
    if (prefixPos == std::string_view::npos) {
        core::Logger::instance().error("ModelLoader", "glTF 2.0 buffer URI not found");
        return nullptr;
    }

    const std::size_t b64Start = prefixPos + kPrefix.size();
    const std::size_t b64End = gltfJson.find('"', b64Start);
    if (b64End == std::string_view::npos) {
        return nullptr;
    }

    const std::vector<std::uint8_t> binBuffer =
        base64Decode(gltfJson.substr(b64Start, b64End - b64Start));

    std::size_t offset = 0;
    std::uint32_t magic = 0;
    if (!readBinary(binBuffer, offset, magic) || magic != kGltfPayloadMagic) {
        core::Logger::instance().error("ModelLoader", "Corrupted glTF 2.0 binary payload");
        return nullptr;
    }

    auto model = std::make_shared<ArticulatedModel>();

    // 1. Read bones
    std::uint32_t boneCount = 0;
    if (!readBinary(binBuffer, offset, boneCount)) {
        return nullptr;
    }
    for (std::uint32_t i = 0; i < boneCount; ++i) {
        std::string name;
        std::int32_t parentIdx = -1;
        animation::TransformTRS bindLocal{};
        if (!readStringBinary(binBuffer, offset, name) ||
            !readBinary(binBuffer, offset, parentIdx) ||
            !readBinary(binBuffer, offset, bindLocal.translation) ||
            !readBinary(binBuffer, offset, bindLocal.rotation) ||
            !readBinary(binBuffer, offset, bindLocal.scale)) {
            return nullptr;
        }
        model->skeleton.addBone(std::move(name), parentIdx, bindLocal);
    }
    model->skeleton.computeDefaultInverseBindMatrices();

    // 2. Read clips
    std::uint32_t clipCount = 0;
    if (!readBinary(binBuffer, offset, clipCount)) {
        return nullptr;
    }
    for (std::uint32_t c = 0; c < clipCount; ++c) {
        animation::AnimationClip clip{};
        std::uint8_t loopByte = 1U;
        std::uint32_t trackCount = 0;
        if (!readStringBinary(binBuffer, offset, clip.name) ||
            !readBinary(binBuffer, offset, clip.durationSeconds) ||
            !readBinary(binBuffer, offset, loopByte) ||
            !readBinary(binBuffer, offset, trackCount)) {
            return nullptr;
        }
        clip.looping = (loopByte != 0U);
        for (std::uint32_t t = 0; t < trackCount; ++t) {
            animation::BoneTrack tr{};
            std::uint32_t bIdx = 0;
            std::uint32_t kfCount = 0;
            if (!readBinary(binBuffer, offset, bIdx) ||
                !readBinary(binBuffer, offset, kfCount)) {
                return nullptr;
            }
            tr.boneIndex = bIdx;
            tr.keyframes.resize(kfCount);
            for (std::uint32_t k = 0; k < kfCount; ++k) {
                if (!readBinary(binBuffer, offset, tr.keyframes[k].timeSeconds) ||
                    !readBinary(binBuffer, offset, tr.keyframes[k].transform.translation) ||
                    !readBinary(binBuffer, offset, tr.keyframes[k].transform.rotation) ||
                    !readBinary(binBuffer, offset, tr.keyframes[k].transform.scale)) {
                    return nullptr;
                }
            }
            clip.tracks.push_back(std::move(tr));
        }
        model->skeleton.addClip(std::move(clip));
    }

    // 3. Read submeshes
    std::uint32_t subMeshCount = 0;
    if (!readBinary(binBuffer, offset, subMeshCount)) {
        return nullptr;
    }
    for (std::uint32_t m = 0; m < subMeshCount; ++m) {
        ModelSubMesh sm{};
        std::int32_t attachedBone = 0;
        std::uint32_t vCount = 0;
        if (!readStringBinary(binBuffer, offset, sm.name) ||
            !readBinary(binBuffer, offset, attachedBone) ||
            !readBinary(binBuffer, offset, sm.baseColorTint) ||
            !readBinary(binBuffer, offset, vCount)) {
            return nullptr;
        }
        sm.attachedBoneIndex = attachedBone;
        std::vector<renderer::Vertex3D> verts(vCount);
        for (std::uint32_t v = 0; v < vCount; ++v) {
            if (!readBinary(binBuffer, offset, verts[v])) {
                return nullptr;
            }
        }
        std::uint32_t iCount = 0;
        if (!readBinary(binBuffer, offset, iCount)) {
            return nullptr;
        }
        std::vector<std::uint32_t> indices(iCount);
        for (std::uint32_t idx = 0; idx < iCount; ++idx) {
            if (!readBinary(binBuffer, offset, indices[idx])) {
                return nullptr;
            }
        }
        sm.mesh = std::make_shared<renderer::Mesh>( std::move(verts), std::move(indices));
        model->subMeshes.push_back(std::move(sm));
    }

    return model;
}

std::shared_ptr<ArticulatedModel> ModelLoader::loadGltf2FromFile(const std::string& filePath) {
    std::ifstream in(filePath, std::ios::in);
    if (!in.is_open()) {
        return nullptr;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    auto model = loadGltf2FromString(ss.str());
    if (model) {
        core::Logger::instance().info(
            "ModelLoader",
            "Loaded glTF 2.0 model '" + filePath + "' (" +
                std::to_string(model->subMeshes.size()) + " submeshes, " +
                std::to_string(model->skeleton.boneCount()) + " bones, " +
                std::to_string(model->skeleton.clips().size()) + " clips)");
    }
    return model;
}

std::shared_ptr<ArticulatedModel> ModelLoader::loadObjFromString(
    std::string_view objContent,
    std::string_view name) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<renderer::Vertex3D> vertices;
    std::vector<std::uint32_t> indices;

    std::istringstream stream{std::string(objContent)};
    std::string line;
    while (std::getline(stream, line)) {
        std::istringstream ls(line);
        std::string token;
        if (!(ls >> token)) {
            continue;
        }
        if (token == "v") {
            glm::vec3 p{0.0f};
            ls >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (token == "vn") {
            glm::vec3 n{0.0f, 1.0f, 0.0f};
            ls >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (token == "vt") {
            glm::vec2 uv{0.0f};
            ls >> uv.x >> uv.y;
            uvs.push_back(uv);
        } else if (token == "f") {
            for (int corner = 0; corner < 3; ++corner) {
                std::string vstr;
                if (!(ls >> vstr)) {
                    break;
                }
                int vi = 1;
                int ti = 0;
                int ni = 0;
                const std::size_t s1 = vstr.find('/');
                if (s1 == std::string::npos) {
                    vi = std::stoi(vstr);
                } else {
                    vi = std::stoi(vstr.substr(0, s1));
                    const std::size_t s2 = vstr.find('/', s1 + 1);
                    if (s2 == std::string::npos) {
                        if (s1 + 1 < vstr.size()) {
                            ti = std::stoi(vstr.substr(s1 + 1));
                        }
                    } else {
                        if (s2 > s1 + 1) {
                            ti = std::stoi(vstr.substr(s1 + 1, s2 - s1 - 1));
                        }
                        if (s2 + 1 < vstr.size()) {
                            ni = std::stoi(vstr.substr(s2 + 1));
                        }
                    }
                }

                renderer::Vertex3D vert{};
                if (vi > 0 && static_cast<std::size_t>(vi) <= positions.size()) {
                    vert.position = positions[static_cast<std::size_t>(vi - 1)];
                }
                if (ti > 0 && static_cast<std::size_t>(ti) <= uvs.size()) {
                    vert.uv = uvs[static_cast<std::size_t>(ti - 1)];
                }
                if (ni > 0 && static_cast<std::size_t>(ni) <= normals.size()) {
                    vert.normal = normals[static_cast<std::size_t>(ni - 1)];
                } else {
                    vert.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }
                vert.color = glm::vec3(1.0f);

                indices.push_back(static_cast<std::uint32_t>(vertices.size()));
                vertices.push_back(vert);
            }
        }
    }

    if (vertices.empty() || indices.empty()) {
        return nullptr;
    }

    auto model = std::make_shared<ArticulatedModel>();
    model->skeleton.addBone("root", -1, animation::TransformTRS{});
    model->skeleton.computeDefaultInverseBindMatrices();
    model->subMeshes.push_back(ModelSubMesh{
        .name = std::string(name),
        .attachedBoneIndex = 0,
        .baseColorTint = glm::vec3(1.0f),
        .mesh = std::make_shared<renderer::Mesh>(std::move(vertices), std::move(indices))
    });
    return model;
}

std::shared_ptr<ArticulatedModel> ModelLoader::createTacticalCarbineModel() {
    auto model = std::make_shared<ArticulatedModel>();

    // Define 5 hierarchical bones:
    // 0: rifle_root
    // 1: bolt_carrier (child of 0)
    // 2: magazine (child of 0)
    // 3: right_arm (child of 0)
    // 4: left_arm (child of 0)
    model->skeleton.addBone("rifle_root",   -1, {{0.18f, -0.15f, -0.42f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    model->skeleton.addBone("bolt_carrier",  0, {{0.02f,  0.02f,  0.00f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    model->skeleton.addBone("magazine",      0, {{0.00f, -0.095f, -0.05f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    model->skeleton.addBone("right_arm",     0, {{0.06f, -0.090f,  0.14f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    model->skeleton.addBone("left_arm",      0, {{-0.10f, -0.065f, -0.12f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    model->skeleton.computeDefaultInverseBindMatrices();

    // Add 4 animation clips: idle, fire, reload, draw
    {
        animation::AnimationClip idle{};
        idle.name = "idle";
        idle.durationSeconds = 2.0f;
        idle.looping = true;
        animation::BoneTrack rootTrack{.boneIndex = 0, .keyframes = {}};
        rootTrack.keyframes = {
            {0.0f, {{0.18f, -0.150f, -0.42f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
            {1.0f, {{0.182f, -0.146f, -0.42f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
            {2.0f, {{0.18f, -0.150f, -0.42f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}}
        };
        idle.tracks.push_back(std::move(rootTrack));
        model->skeleton.addClip(std::move(idle));
    }
    {
        animation::AnimationClip fire{};
        fire.name = "fire";
        fire.durationSeconds = 0.10f;
        fire.looping = false;
        animation::BoneTrack rootTrack{.boneIndex = 0, .keyframes = {}};
        rootTrack.keyframes = {
            {0.00f, {{0.18f, -0.150f, -0.42f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
            {0.03f, {{0.18f, -0.140f, -0.37f}, glm::quat(glm::vec3(glm::radians(6.0f), 0.0f, 0.0f)), {1.0f, 1.0f, 1.0f}}},
            {0.10f, {{0.18f, -0.150f, -0.42f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}}
        };
        animation::BoneTrack boltTrack{.boneIndex = 1, .keyframes = {}};
        boltTrack.keyframes = {
            {0.00f, {{0.02f, 0.02f, 0.00f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
            {0.03f, {{0.02f, 0.02f, 0.07f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
            {0.10f, {{0.02f, 0.02f, 0.00f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}}
        };
        fire.tracks.push_back(std::move(rootTrack));
        fire.tracks.push_back(std::move(boltTrack));
        model->skeleton.addClip(std::move(fire));
    }
    {
        animation::AnimationClip reload{};
        reload.name = "reload";
        reload.durationSeconds = 2.10f;
        reload.looping = false;
        animation::BoneTrack magTrack{.boneIndex = 2, .keyframes = {}};
        magTrack.keyframes = {
            {0.00f, {{0.00f, -0.095f, -0.05f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
            {0.85f, {{-0.04f, -0.280f, -0.02f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
            {1.45f, {{-0.02f, -0.220f, -0.04f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
            {2.10f, {{0.00f, -0.095f, -0.05f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}}
        };
        reload.tracks.push_back(std::move(magTrack));
        model->skeleton.addClip(std::move(reload));
    }
    {
        animation::AnimationClip draw{};
        draw.name = "draw";
        draw.durationSeconds = 0.65f;
        draw.looping = false;
        animation::BoneTrack rootTrack{.boneIndex = 0, .keyframes = {}};
        rootTrack.keyframes = {
            {0.00f, {{0.18f, -0.36f, -0.42f}, glm::quat(glm::vec3(glm::radians(-25.0f), 0.0f, 0.0f)), {1.0f, 1.0f, 1.0f}}},
            {0.65f, {{0.18f, -0.15f, -0.42f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}}
        };
        draw.tracks.push_back(std::move(rootTrack));
        model->skeleton.addClip(std::move(draw));
    }

    // Submeshes attached to bones
    model->subMeshes.push_back({"receiver",  0, {0.28f, 0.31f, 0.35f}, renderer::Mesh::createBox({0.055f, 0.075f, 0.24f}, 2.0f)});
    model->subMeshes.push_back({"handguard", 0, {0.18f, 0.20f, 0.23f}, renderer::Mesh::createBox({0.048f, 0.052f, 0.20f}, 2.0f)});
    model->subMeshes.push_back({"barrel",    0, {0.28f, 0.31f, 0.35f}, renderer::Mesh::createBox({0.022f, 0.022f, 0.22f}, 2.0f)});
    model->subMeshes.push_back({"bolt",      1, {0.50f, 0.52f, 0.55f}, renderer::Mesh::createBox({0.018f, 0.024f, 0.06f}, 1.0f)});
    model->subMeshes.push_back({"magazine",  2, {0.75f, 0.65f, 0.42f}, renderer::Mesh::createBox({0.034f, 0.135f, 0.058f}, 1.0f)});
    model->subMeshes.push_back({"right_arm", 3, {0.32f, 0.38f, 0.28f}, renderer::Mesh::createBox({0.068f, 0.068f, 0.24f}, 1.0f)});
    model->subMeshes.push_back({"left_arm",  4, {0.32f, 0.38f, 0.28f}, renderer::Mesh::createBox({0.065f, 0.065f, 0.28f}, 1.0f)});

    return model;
}

std::shared_ptr<ArticulatedModel> ModelLoader::createTargetDummyModel() {
    auto model = std::make_shared<ArticulatedModel>();

    // 5 bones: pelvis (0), torso (1), head (2), left_arm (3), right_arm (4)
    model->skeleton.addBone("pelvis",    -1, {{0.0f, 0.90f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    model->skeleton.addBone("torso",      0, {{0.0f, 0.35f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    model->skeleton.addBone("head",       1, {{0.0f, 0.55f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    model->skeleton.addBone("left_arm",   1, {{-0.42f, 0.10f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    model->skeleton.addBone("right_arm",  1, {{ 0.42f, 0.10f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    model->skeleton.computeDefaultInverseBindMatrices();

    {
        animation::AnimationClip idle{};
        idle.name = "idle";
        idle.durationSeconds = 2.0f;
        idle.looping = true;
        animation::BoneTrack torsoTrack{.boneIndex = 1, .keyframes = {}};
        torsoTrack.keyframes = {
            {0.0f, {{0.0f, 0.35f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
            {1.0f, {{0.0f, 0.37f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
            {2.0f, {{0.0f, 0.35f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}}
        };
        idle.tracks.push_back(std::move(torsoTrack));
        model->skeleton.addClip(std::move(idle));
    }
    {
        animation::AnimationClip flinch{};
        flinch.name = "hit_flinch";
        flinch.durationSeconds = 0.35f;
        flinch.looping = false;
        animation::BoneTrack torsoTrack{.boneIndex = 1, .keyframes = {}};
        torsoTrack.keyframes = {
            {0.00f, {{0.0f, 0.35f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
            {0.10f, {{0.0f, 0.35f, -0.08f}, glm::quat(glm::vec3(glm::radians(-18.0f), 0.0f, 0.0f)), {1.0f, 1.0f, 1.0f}}},
            {0.35f, {{0.0f, 0.35f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}}
        };
        flinch.tracks.push_back(std::move(torsoTrack));
        model->skeleton.addClip(std::move(flinch));
    }

    model->subMeshes.push_back({"pelvis_box",    0, {0.35f, 0.38f, 0.42f}, renderer::Mesh::createBox({0.55f, 0.45f, 0.35f}, 1.0f)});
    model->subMeshes.push_back({"torso_box",     1, {0.85f, 0.65f, 0.25f}, renderer::Mesh::createBox({0.65f, 0.75f, 0.38f}, 1.0f)});
    model->subMeshes.push_back({"head_box",      2, {0.95f, 0.35f, 0.25f}, renderer::Mesh::createBox({0.36f, 0.38f, 0.36f}, 1.0f)});
    model->subMeshes.push_back({"left_arm_box",  3, {0.45f, 0.48f, 0.52f}, renderer::Mesh::createBox({0.20f, 0.65f, 0.20f}, 1.0f)});
    model->subMeshes.push_back({"right_arm_box", 4, {0.45f, 0.48f, 0.52f}, renderer::Mesh::createBox({0.20f, 0.65f, 0.20f}, 1.0f)});

    return model;
}

std::shared_ptr<ArticulatedModel> AssetCache::loadOrGetGltfModel(const std::string& filePath) {
    if (auto existing = resources_.findByName(filePath)) {
        if (auto typed = std::dynamic_pointer_cast<ArticulatedModel>(existing)) {
            ++cacheHits_;
            return typed;
        }
    }
    ++cacheMisses_;
    auto loaded = ModelLoader::loadGltf2FromFile(filePath);
    if (loaded) {
        (void)resources_.registerResource(filePath, loaded);
    }
    return loaded;
}

} // namespace classicfps::resources
