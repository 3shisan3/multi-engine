/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        ResourceManager.cpp
Description: 资源管理模块实现

*****************************************************************/

#include "resource_impl.h"

#include <algorithm>

namespace ResourceModule
{

bool ResourceManagerImpl::RegisterProvider(std::shared_ptr<IResourceProvider> provider)
{
    if (!provider || provider->GetType().empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto type = provider->GetType();
    if (providers_.find(type) != providers_.end())
    {
        return false;
    }

    providers_[type] = std::move(provider);
    return true;
}

bool ResourceManagerImpl::UnregisterProvider(const ResourceType& type)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return providers_.erase(type) > 0;
}

std::shared_ptr<IResourceProvider> ResourceManagerImpl::FindProvider(const ResourceType& type) const
{
    auto it = providers_.find(type);
    return it == providers_.end() ? nullptr : it->second;
}

std::shared_ptr<IResource> ResourceManagerImpl::Acquire(
    const ResourceId& id,
    const std::map<std::string, std::any>& params)
{
    if (id.empty())
    {
        return nullptr;
    }

    std::shared_ptr<IResourceProvider> provider;
    ResourceType type;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // 命中缓存直接返回
        auto cached = cache_.find(id);
        if (cached != cache_.end() && cached->second.resource)
        {
            return cached->second.resource;
        }

        // 未命中：需要一个能处理该资源的 provider。
        // 缓存未命中时通过参数显式指定类型；未指定则尝试唯一 provider。
        auto paramIt = params.find("resource_type");
        if (paramIt != params.end())
        {
            try
            {
                type = std::any_cast<ResourceType>(paramIt->second);
            }
            catch (...)
            {
                return nullptr;
            }
        }
        else if (providers_.size() == 1)
        {
            type = providers_.begin()->first;
        }
        else
        {
            return nullptr;
        }

        provider = FindProvider(type);
        if (!provider)
        {
            return nullptr;
        }

        loadAttempts_++;
    }

    // 在锁外执行实际加载，避免长时间持有锁
    std::shared_ptr<IResource> resource = provider->Load(id, params);

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!resource)
        {
            loadFailures_++;
            return nullptr;
        }

        CacheEntry entry;
        entry.resource = resource;
        entry.type = type;
        cache_[id] = std::move(entry);
    }

    return resource;
}

std::shared_ptr<IResource> ResourceManagerImpl::Get(const ResourceId& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(id);
    return it == cache_.end() ? nullptr : it->second.resource;
}

bool ResourceManagerImpl::Release(const ResourceId& id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.erase(id) > 0;
}

size_t ResourceManagerImpl::ReleaseUnused()
{
    std::lock_guard<std::mutex> lock(mutex_);

    size_t released = 0;
    for (auto it = cache_.begin(); it != cache_.end(); )
    {
        // use_count == 1 表示仅由缓存持有，外部无引用
        if (it->second.resource.use_count() == 1)
        {
            it = cache_.erase(it);
            released++;
        }
        else
        {
            ++it;
        }
    }

    return released;
}

void ResourceManagerImpl::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
    loadAttempts_ = 0;
    loadFailures_ = 0;
}

std::vector<ResourceId> ResourceManagerImpl::GetCachedIds() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ResourceId> ids;
    ids.reserve(cache_.size());
    for (const auto& pair : cache_)
    {
        ids.push_back(pair.first);
    }

    std::sort(ids.begin(), ids.end());
    return ids;
}

bool ResourceManagerImpl::IsCached(const ResourceId& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.find(id) != cache_.end();
}

ResourceStats ResourceManagerImpl::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    ResourceStats stats;
    stats.cachedCount = cache_.size();
    stats.providerCount = providers_.size();
    stats.loadAttempts = loadAttempts_;
    stats.loadFailures = loadFailures_;

    for (const auto& pair : cache_)
    {
        // 引用计数大于 1 说明外部仍持有
        if (pair.second.resource.use_count() > 1)
        {
            stats.activeCount++;
        }
    }

    return stats;
}

long ResourceManagerImpl::GetCacheUseCount(const ResourceId& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(id);
    if (it == cache_.end())
    {
        return 0;
    }
    return it->second.resource.use_count();
}

std::shared_ptr<IResourceManager> CreateResourceManager()
{
    return std::make_shared<ResourceManagerImpl>();
}

} // namespace ResourceModule
