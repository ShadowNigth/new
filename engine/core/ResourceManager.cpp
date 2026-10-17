#include "engine/core/ResourceManager.hpp"
#include "engine/core/Logger.hpp"

#include <sstream>
#include <utility>

namespace classicfps::core {

ResourceManager::~ResourceManager() {
    releaseAll();
}

ResourceId ResourceManager::registerResource(
    std::string_view name,
    std::shared_ptr<IResource> resource) {
    if (!resource || name.empty()) {
        Logger::instance().error("ResourceManager", "Attempted to register null or unnamed resource");
        return kInvalidResourceId;
    }

    const std::string key(name);
    const auto existingIt = idByName_.find(key);
    if (existingIt != idByName_.end()) {
        const ResourceId id = existingIt->second;
        byId_[id].resource = std::move(resource);
        Logger::instance().debug("ResourceManager", "Replaced existing resource: " + key);
        return id;
    }

    const ResourceId id = nextId_++;
    byId_.emplace(id, Entry{.id = id, .name = key, .resource = std::move(resource)});
    idByName_.emplace(key, id);

    std::ostringstream oss;
    oss << "Registered resource '" << key << "' (id=" << id << ")";
    Logger::instance().debug("ResourceManager", oss.str());
    return id;
}

std::shared_ptr<IResource> ResourceManager::findById(ResourceId id) const {
    const auto it = byId_.find(id);
    if (it == byId_.end()) {
        return nullptr;
    }
    return it->second.resource;
}

std::shared_ptr<IResource> ResourceManager::findByName(std::string_view name) const {
    const auto idIt = idByName_.find(std::string(name));
    if (idIt == idByName_.end()) {
        return nullptr;
    }
    return findById(idIt->second);
}

bool ResourceManager::releaseById(ResourceId id) {
    const auto it = byId_.find(id);
    if (it == byId_.end()) {
        return false;
    }
    idByName_.erase(it->second.name);
    byId_.erase(it);
    return true;
}

bool ResourceManager::releaseByName(std::string_view name) {
    const auto idIt = idByName_.find(std::string(name));
    if (idIt == idByName_.end()) {
        return false;
    }
    const ResourceId id = idIt->second;
    idByName_.erase(idIt);
    byId_.erase(id);
    return true;
}

void ResourceManager::releaseAll() noexcept {
    if (!byId_.empty()) {
        std::ostringstream oss;
        oss << "Releasing " << byId_.size() << " managed resource(s) ("
            << totalMemoryBytes() << " bytes)";
        Logger::instance().info("ResourceManager", oss.str());
    }
    idByName_.clear();
    byId_.clear();
}

std::size_t ResourceManager::totalMemoryBytes() const noexcept {
    std::size_t total = 0;
    for (const auto& [_, entry] : byId_) {
        if (entry.resource) {
            total += entry.resource->memoryBytes();
        }
    }
    return total;
}

} // namespace classicfps::core
