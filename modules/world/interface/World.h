/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        World.h
Version:     1.0
Author:      cjx
start date: 2026-10-23
Description: 世界模型模块对外接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-10-23      cjx        create

*****************************************************************/

#ifndef ME_WORLD_WORLD_H
#define ME_WORLD_WORLD_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace WorldModule
{

using EntityId = uint64_t;

/**
 * @brief 三维位置
 */
struct Vector3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    double DistanceSquared(const Vector3& other) const;
    double Distance(const Vector3& other) const;
};

/**
 * @brief 实体接口
 *
 * 具体实体（舰船、飞机、车辆等）继承此接口。
 */
class IEntity
{
public:
    virtual ~IEntity() = default;

    virtual EntityId GetId() const = 0;
    virtual std::string GetType() const = 0;
    virtual Vector3 GetPosition() const = 0;
    virtual void SetPosition(const Vector3& position) = 0;
};

/**
 * @brief 世界快照
 */
struct WorldSnapshot
{
    uint64_t snapshotId = 0;
    int64_t timestamp = 0;
    std::map<EntityId, std::string> entityTypes;
    std::map<EntityId, Vector3> entityPositions;
};

/**
 * @brief 世界模型统计
 */
struct WorldStatistics
{
    size_t entityCount = 0;
    uint64_t addCount = 0;
    uint64_t removeCount = 0;
    uint64_t snapshotCount = 0;
};

/**
 * @brief 世界模型接口
 *
 * 维护仿真世界中的实体集合，提供空间查询与快照能力。
 * 线程安全。
 */
class IWorldModel
{
public:
    virtual ~IWorldModel() = default;

    virtual bool AddEntity(std::shared_ptr<IEntity> entity) = 0;
    virtual bool RemoveEntity(EntityId id) = 0;
    virtual std::shared_ptr<IEntity> GetEntity(EntityId id) const = 0;
    virtual std::vector<std::shared_ptr<IEntity>> GetEntities() const = 0;

    /**
     * @brief 半径查询：返回中心点 radius 范围内的实体
     */
    virtual std::vector<std::shared_ptr<IEntity>> QueryByRadius(
        const Vector3& center, double radius) const = 0;

    virtual std::vector<std::shared_ptr<IEntity>> QueryByType(const std::string& type) const = 0;

    virtual size_t GetEntityCount() const = 0;
    virtual void Clear() = 0;

    /** @brief 捕获快照 */
    virtual WorldSnapshot CaptureSnapshot() = 0;

    /** @brief 从快照恢复（清空现有实体后由调用方重建实体） */
    virtual bool ApplySnapshot(const WorldSnapshot& snapshot) = 0;

    virtual WorldStatistics GetStatistics() const = 0;

    /** @brief 注册实体变更回调 */
    virtual void SetEntityChangeCallback(
        std::function<void(EntityId id, const std::string& type, bool added)> callback) = 0;
};

/**
 * @brief 创建世界模型实例
 */
std::shared_ptr<IWorldModel> CreateWorldModel();

} // namespace WorldModule

#endif // ME_WORLD_WORLD_H
