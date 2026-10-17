#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace classicfps::core {

using ResourceId = std::uint64_t;
constexpr ResourceId kInvalidResourceId = 0;

class IResource {
public:
    virtual ~IResource() = default;
    [[nodiscard]] virtual std::string_view typeName() const noexcept = 0;
    [[nodiscard]] virtual std::size_t memoryBytes() const noexcept = 0;
};

class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    ResourceId registerResource(std::string_view name, std::shared_ptr<IResource> resource);
    [[nodiscard]] std::shared_ptr<IResource> findById(ResourceId id) const;
    [[nodiscard]] std::shared_ptr<IResource> findByName(std::string_view name) const;

    bool releaseById(ResourceId id);
    bool releaseByName(std::string_view name);
    void releaseAll() noexcept;

    [[nodiscard]] std::size_t activeCount() const noexcept { return byId_.size(); }
    [[nodiscard]] std::size_t totalMemoryBytes() const noexcept;

private:
    struct Entry {
        ResourceId id{kInvalidResourceId};
        std::string name;
        std::shared_ptr<IResource> resource;
    };

    ResourceId nextId_{1};
    std::unordered_map<ResourceId, Entry> byId_;
    std::unordered_map<std::string, ResourceId> idByName_;
};

} // namespace classicfps::core
