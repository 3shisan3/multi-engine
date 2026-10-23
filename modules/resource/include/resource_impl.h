#ifndef ME_RESOURCE_IMPL_H
#define ME_RESOURCE_IMPL_H

#include "Resource.h"

#include <map>
#include <mutex>
#include <unordered_map>

namespace ResourceModule
{

class ResourceManagerImpl : public IResourceManager
{
public:
    bool RegisterProvider(std::shared_ptr<IResourceProvider> provider) override;
    bool UnregisterProvider(const ResourceType& type) override;

    std::shared_ptr<IResource> Acquire(
        const ResourceId& id,
        const std::map<std::string, std::any>& params = {}) override;

    std::shared_ptr<IResource> Get(const ResourceId& id) const override;

    bool Release(const ResourceId& id) override;
    size_t ReleaseUnused() override;
    void Clear() override;

    std::vector<ResourceId> GetCachedIds() const override;
    bool IsCached(const ResourceId& id) const override;

    ResourceStats GetStatistics() const override;
    long GetCacheUseCount(const ResourceId& id) const override;

private:
    struct CacheEntry
    {
        std::shared_ptr<IResource> resource;
        ResourceType type;
    };

    std::shared_ptr<IResourceProvider> FindProvider(const ResourceType& type) const;

    mutable std::mutex mutex_;
    std::unordered_map<ResourceType, std::shared_ptr<IResourceProvider>> providers_;
    std::unordered_map<ResourceId, CacheEntry> cache_;

    // 统计
    size_t loadAttempts_ = 0;
    size_t loadFailures_ = 0;
};

} // namespace ResourceModule

#endif // ME_RESOURCE_IMPL_H
