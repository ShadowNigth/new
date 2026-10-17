#include "engine/renderer/Texture.hpp"
#include "engine/core/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace classicfps::renderer {

namespace {

[[nodiscard]] std::uint8_t clampByte(int v) noexcept {
    return static_cast<std::uint8_t>(std::clamp(v, 0, 255));
}

[[nodiscard]] int hashNoise(int x, int y, int seed = 1337) noexcept {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393U +
                      static_cast<std::uint32_t>(y) * 668265263U +
                      static_cast<std::uint32_t>(seed) * 1442695041U;
    h = (h ^ (h >> 13U)) * 1274126177U;
    return static_cast<int>((h ^ (h >> 16U)) & 0x1FU) - 16; // [-16, +15]
}

void setPixelRGBA(
    std::vector<std::uint8_t>& buf,
    int width,
    int x,
    int y,
    int r,
    int g,
    int b,
    int a = 255) noexcept {
    const std::size_t idx =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U;
    buf[idx + 0] = clampByte(r);
    buf[idx + 1] = clampByte(g);
    buf[idx + 2] = clampByte(b);
    buf[idx + 3] = clampByte(a);
}

} // namespace

Texture2D::Texture2D(int width, int height, std::vector<std::uint8_t> rgbaPixels)
    : width_(width), height_(height), pixels_(std::move(rgbaPixels)) {}

glm::vec4 Texture2D::sampleNearest(float u, float v) const noexcept {
    if (!isValid()) {
        return glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
    }

    const float wrappedU = u - std::floor(u);
    const float wrappedV = v - std::floor(v);

    const int tx = std::clamp(static_cast<int>(wrappedU * static_cast<float>(width_)), 0, width_ - 1);
    const int ty = std::clamp(static_cast<int>(wrappedV * static_cast<float>(height_)), 0, height_ - 1);

    const std::size_t idx =
        (static_cast<std::size_t>(ty) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(tx)) * 4U;
    return glm::vec4(
        static_cast<float>(pixels_[idx + 0]) / 255.0f,
        static_cast<float>(pixels_[idx + 1]) / 255.0f,
        static_cast<float>(pixels_[idx + 2]) / 255.0f,
        static_cast<float>(pixels_[idx + 3]) / 255.0f);
}

bool Texture2D::loadFromPPM(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        core::Logger::instance().error("Texture2D", "Could not open PPM file: " + filePath);
        return false;
    }

    std::string magic;
    file >> magic;
    if (magic != "P6" && magic != "P3") {
        core::Logger::instance().error("Texture2D", "Unsupported PPM format in " + filePath);
        return false;
    }

    auto skipComments = [&file]() {
        while (file >> std::ws && file.peek() == '#') {
            std::string dummy;
            std::getline(file, dummy);
        }
    };

    skipComments();
    int w = 0;
    int h = 0;
    int maxVal = 0;
    file >> w;
    skipComments();
    file >> h;
    skipComments();
    file >> maxVal;

    if (w <= 0 || h <= 0 || maxVal <= 0) {
        core::Logger::instance().error("Texture2D", "Invalid PPM dimensions in " + filePath);
        return false;
    }

    width_ = w;
    height_ = h;
    pixels_.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4U, 255U);

    if (magic == "P6") {
        file.get(); // consume single whitespace byte after maxVal
        std::vector<std::uint8_t> rgb(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3U);
        file.read(reinterpret_cast<char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
        if (!file) {
            core::Logger::instance().error("Texture2D", "Truncated binary PPM data in " + filePath);
            return false;
        }
        for (std::size_t i = 0, p = 0; i < rgb.size(); i += 3, p += 4) {
            pixels_[p + 0] = rgb[i + 0];
            pixels_[p + 1] = rgb[i + 1];
            pixels_[p + 2] = rgb[i + 2];
            pixels_[p + 3] = 255U;
        }
    } else {
        for (std::size_t p = 0; p < pixels_.size(); p += 4) {
            int r = 0;
            int g = 0;
            int b = 0;
            file >> r >> g >> b;
            pixels_[p + 0] = clampByte(r);
            pixels_[p + 1] = clampByte(g);
            pixels_[p + 2] = clampByte(b);
            pixels_[p + 3] = 255U;
        }
    }

    return true;
}

bool Texture2D::saveToPPM(const std::string& filePath) const {
    if (!isValid()) {
        return false;
    }
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file << "P6\n" << width_ << ' ' << height_ << "\n255\n";
    for (std::size_t p = 0; p < pixels_.size(); p += 4) {
        const char rgb[3] = {
            static_cast<char>(pixels_[p + 0]),
            static_cast<char>(pixels_[p + 1]),
            static_cast<char>(pixels_[p + 2])
        };
        file.write(rgb, 3);
    }
    return file.good();
}

std::shared_ptr<Texture2D> Texture2D::createCheckerboard(int size, int tileSize) {
    const int s = std::max(8, size);
    const int ts = std::max(1, tileSize);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(s) * static_cast<std::size_t>(s) * 4U);

    for (int y = 0; y < s; ++y) {
        for (int x = 0; x < s; ++x) {
            const bool odd = (((x / ts) + (y / ts)) & 1) != 0;
            if (odd) {
                setPixelRGBA(data, s, x, y, 210, 215, 220);
            } else {
                setPixelRGBA(data, s, x, y, 70, 80, 95);
            }
        }
    }
    return std::make_shared<Texture2D>(s, s, std::move(data));
}

std::shared_ptr<Texture2D> Texture2D::createConcreteWall(int size) {
    const int s = std::max(16, size);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(s) * static_cast<std::size_t>(s) * 4U);

    for (int y = 0; y < s; ++y) {
        for (int x = 0; x < s; ++x) {
            const int n = hashNoise(x, y, 101);
            int base = 138 + n / 2;
            // Panel seams and rivets
            if (x == 0 || y == 0 || x == s / 2 || y == s / 2) {
                base -= 36;
            } else if (x == 1 || y == 1 || x == (s / 2 + 1) || y == (s / 2 + 1)) {
                base += 16;
            }
            setPixelRGBA(data, s, x, y, base, base + 2, base + 4);
        }
    }
    return std::make_shared<Texture2D>(s, s, std::move(data));
}

std::shared_ptr<Texture2D> Texture2D::createBrickWall(int size) {
    const int s = std::max(16, size);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(s) * static_cast<std::size_t>(s) * 4U);

    const int brickH = std::max(4, s / 8);
    const int brickW = std::max(8, s / 4);

    for (int y = 0; y < s; ++y) {
        const int row = y / brickH;
        const int offsetX = (row & 1) ? (brickW / 2) : 0;
        for (int x = 0; x < s; ++x) {
            const int localY = y % brickH;
            const int localX = (x + offsetX) % brickW;
            const int n = hashNoise(x, y, 202);

            if (localY == 0 || localX == 0) {
                // Mortar line
                const int m = 82 + n / 3;
                setPixelRGBA(data, s, x, y, m, m - 2, m - 5);
            } else {
                const int brickSeed = hashNoise(row, (x + offsetX) / brickW, 777);
                const int r = 152 + brickSeed + n / 2;
                const int g = 78 + brickSeed / 2 + n / 3;
                const int b = 58 + brickSeed / 3 + n / 4;
                setPixelRGBA(data, s, x, y, r, g, b);
            }
        }
    }
    return std::make_shared<Texture2D>(s, s, std::move(data));
}

std::shared_ptr<Texture2D> Texture2D::createFloorTile(int size) {
    const int s = std::max(16, size);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(s) * static_cast<std::size_t>(s) * 4U);
    const int tile = std::max(8, s / 4);

    for (int y = 0; y < s; ++y) {
        for (int x = 0; x < s; ++x) {
            const int lx = x % tile;
            const int ly = y % tile;
            const int n = hashNoise(x, y, 303);
            const bool alt = (((x / tile) + (y / tile)) & 1) != 0;

            int base = (alt ? 98 : 112) + n / 3;
            if (lx == 0 || ly == 0) {
                base -= 28;
            } else if (lx == tile - 1 || ly == tile - 1) {
                base += 14;
            }
            setPixelRGBA(data, s, x, y, base - 4, base, base + 8);
        }
    }
    return std::make_shared<Texture2D>(s, s, std::move(data));
}

std::shared_ptr<Texture2D> Texture2D::createCeilingPanel(int size) {
    const int s = std::max(16, size);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(s) * static_cast<std::size_t>(s) * 4U);

    for (int y = 0; y < s; ++y) {
        for (int x = 0; x < s; ++x) {
            const int n = hashNoise(x, y, 404);
            int v = 86 + n / 4;
            if ((x % 16) == 0 || (y % 16) == 0) {
                v = 52;
            }
            setPixelRGBA(data, s, x, y, v, v + 5, v + 12);
        }
    }
    return std::make_shared<Texture2D>(s, s, std::move(data));
}

std::shared_ptr<Texture2D> Texture2D::createHazardStripes(int size) {
    const int s = std::max(16, size);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(s) * static_cast<std::size_t>(s) * 4U);

    for (int y = 0; y < s; ++y) {
        for (int x = 0; x < s; ++x) {
            const int n = hashNoise(x, y, 505) / 3;
            const bool yellow = (((x + y) / 8) & 1) == 0;
            if (yellow) {
                setPixelRGBA(data, s, x, y, 222 + n, 182 + n, 28 + n);
            } else {
                setPixelRGBA(data, s, x, y, 36 + n, 36 + n, 40 + n);
            }
        }
    }
    return std::make_shared<Texture2D>(s, s, std::move(data));
}

std::shared_ptr<Texture2D> Texture2D::createCrateWood(int size) {
    const int s = std::max(16, size);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(s) * static_cast<std::size_t>(s) * 4U);

    for (int y = 0; y < s; ++y) {
        for (int x = 0; x < s; ++x) {
            const int grain = hashNoise(x / 2, y, 606) / 2;
            const bool frame = (x < 6 || x >= s - 6 || y < 6 || y >= s - 6 ||
                                std::abs(x - y) < 4);
            const bool plankSeam = (y % (s / 4)) == 0;

            int r = frame ? 142 : 168;
            int g = frame ? 102 : 124;
            int b = frame ? 58 : 72;

            if (plankSeam && !frame) {
                r -= 38;
                g -= 30;
                b -= 20;
            }
            setPixelRGBA(data, s, x, y, r + grain, g + grain, b + grain / 2);
        }
    }
    return std::make_shared<Texture2D>(s, s, std::move(data));
}

} // namespace classicfps::renderer
