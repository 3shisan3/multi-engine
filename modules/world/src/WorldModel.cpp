/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        WorldModel.cpp
Description: 世界模型模块实现

*****************************************************************/

#include "world_impl.h"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace std::chrono;

namespace WorldModule
{

double Vector3::DistanceSquared(const Vector3& other) const
{
    const double dx = x - other.x;
    const double dy = y - other.y;
    const double dz = z - other.z;
    return dx * dx + dy * dy + dz * dz;
}

double Vector3::Distance(const Vector3& other) const
{
    return std::sqrt(DistanceSquared(other));
}

bool WorldModelImpl::AddEntity(std::shared_ptr<IEntity> entity)
{
    if (!entity)
    {
        return false;
    }

    const EntityId id = entity->GetId();
    const std::string type = entity->GetType();
    std::function<void(EntityId, const std::string&, bool)> callback;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (entities_.find(id) != entities_.end())
        {
            return false;
        }

        entities_[id] = entity;
        statistics_.entityCount = entities_.size();
        statistics_.addCount++;

        callback = changeCallback_;
    }

    // 锁外通知，避免回调重入导致死锁
    if (callback)
    {
        callback(id, type, true);
    }
    return true;
}

bool WorldModelImpl::RemoveEntity(EntityId id)
{
    std::string type;
    std::function<void(EntityId, const std::string&, bool)> callback;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = entities_.find(id);
        if (it == entities_.end())
        {
            return false;
        }

        type = it->second->GetType();
        entities_.erase(it);
        statistics_.entityCount = entities_.size();
        statistics_.removeCount++;

        callback = changeCallback_;
    }

    if (callback)
    {
        callback(id, type, false);
    }
    return true;
}

std::shared_ptr<IEntity> WorldModelImpl::GetEntity(EntityId id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : it->second;
}

std::vector<std::shared_ptr<IEntity>> WorldModelImpl::GetEntities() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<IEntity>> result;
    result.reserve(entities_.size());
    for (const auto& pair : entities_)
    {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<std::shared_ptr<IEntity>> WorldModelImpl::QueryByRadius(
    const Vector3& center, double radius) const
{
    if (radius < 0.0)
    {
        return {};
    }

    const double radiusSquared = radius * radius;

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<IEntity>> result;
    for (const auto& pair : entities_)
    {
        if (pair.second->GetPosition().DistanceSquared(center) <= radiusSquared)
        {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<IEntity>> WorldModelImpl::QueryByType(const std::string& type) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<IEntity>> result;
    for (const auto& pair : entities_)
    {
        if (pair.second->GetType() == type)
        {
            result.push_back(pair.second);
        }
    }
    return result;
}

size_t WorldModelImpl::GetEntityCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return entities_.size();
}

void WorldModelImpl::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    entities_.clear();
    statistics_.entityCount = 0;
}

WorldSnapshot WorldModelImpl::CaptureSnapshot()
{
    std::lock_guard<std::mutex> lock(mutex_);

    WorldSnapshot snapshot;
    snapshot.snapshotId = nextSnapshotId_++;
    snapshot.timestamp = duration_cast<milliseconds>(
                            system_clock::now().time_since_epoch())
                            .count();

    for (const auto& pair : entities_)
    {
        snapshot.entityTypes[pair.first] = pair.second->GetType();
        snapshot.entityPositions[pair.first] = pair.second->GetPosition();
    }

    statistics_.snapshotCount++;
    return snapshot;
}

bool WorldModelImpl::ApplySnapshot(const WorldSnapshot& snapshot)
{
    std::lock_guard<std::mutex> lock(mutex_);

    entities_.clear();
    statistics_.entityCount = 0;

    // 快照仅记录 ID 与类型/位置，实体实例由调用方依据快照重建
    for (const auto& pair : snapshot.entityTypes)
    {
        statistics_.entityCount++;
        (void)pair;
    }

    return true;
}

WorldStatistics WorldModelImpl::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return statistics_;
}

void WorldModelImpl::SetEntityChangeCallback(
    std::function<void(EntityId id, const std::string& type, bool added)> callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    changeCallback_ = std::move(callback);
}

void WorldModelImpl::NotifyChange(EntityId id, const std::string& type, bool added)
{
    if (changeCallback_)
    {
        changeCallback_(id, type, added);
    }
}

std::shared_ptr<IWorldModel> CreateWorldModel()
{
    return std::make_shared<WorldModelImpl>();
}

} // namespace WorldModule
